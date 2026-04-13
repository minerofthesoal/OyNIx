#!/bin/bash
# ============================================================
#  OyNIx Browser — Ubuntu / Linux Mint Build Script
#  Builds the browser from source and optionally installs it.
#
#  Usage:
#    ./scripts/build-ubuntu.sh              # build only
#    ./scripts/build-ubuntu.sh --install    # build + install to ~/.local
#    ./scripts/build-ubuntu.sh --system     # build + install to /usr (needs sudo)
#    ./scripts/build-ubuntu.sh --deb        # build .deb package
#    ./scripts/build-ubuntu.sh --deps-only  # install dependencies only
#    ./scripts/build-ubuntu.sh --clean      # remove build artifacts
#
#  Environment:
#    QT_PREFIX   — Qt6 install prefix (auto-detected)
#    JOBS        — parallel build jobs (default: nproc)
#    BUILD_TYPE  — Release or Debug (default: Release)
# ============================================================
set -euo pipefail

# ── Colors ─────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()   { echo -e "${RED}[ERR]${NC}   $*" >&2; }
step()  { echo -e "\n${PURPLE}${BOLD}==> $*${NC}"; }

# ── Defaults ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/_build_ubuntu"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
ACTION="build"

# ── Parse args ─────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --install)    ACTION="install-local" ;;
        --system)     ACTION="install-system" ;;
        --deb)        ACTION="deb" ;;
        --deps-only)  ACTION="deps" ;;
        --clean)      ACTION="clean" ;;
        --debug)      BUILD_TYPE="Debug" ;;
        --help|-h)
            echo "Usage: $0 [--install|--system|--deb|--deps-only|--clean|--debug]"
            echo ""
            echo "Options:"
            echo "  --install    Build and install to ~/.local (no sudo)"
            echo "  --system     Build and install to /usr (needs sudo)"
            echo "  --deb        Build a .deb package"
            echo "  --deps-only  Install build dependencies only"
            echo "  --clean      Remove build artifacts"
            echo "  --debug      Build in Debug mode"
            echo ""
            echo "With no flags, builds the binary in ${BUILD_DIR}/"
            exit 0
            ;;
        *)
            err "Unknown option: $arg"
            exit 1
            ;;
    esac
done

# ── Banner ─────────────────────────────────────────────────────
echo -e "${PURPLE}${BOLD}"
echo "  ╔══════════════════════════════════════╗"
echo "  ║    OyNIx Browser — Ubuntu Builder    ║"
echo "  ║         v3.1 • C++17 + Qt6           ║"
echo "  ╚══════════════════════════════════════╝"
echo -e "${NC}"

# ── Clean ──────────────────────────────────────────────────────
if [ "$ACTION" = "clean" ]; then
    step "Cleaning build artifacts"
    rm -rf "${BUILD_DIR}" "${PROJECT_DIR}/_build_deb" "${PROJECT_DIR}/build"
    rm -f "${PROJECT_DIR}"/*.deb
    ok "Clean complete"
    exit 0
fi

# ── Detect distro ─────────────────────────────────────────────
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "${ID}"
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        echo "${DISTRIB_ID,,}"
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
info "Detected distro: ${DISTRO}"

case "$DISTRO" in
    ubuntu|linuxmint|pop|elementary|zorin|neon|kubuntu|xubuntu|lubuntu)
        PKG_MGR="apt"
        ;;
    debian)
        PKG_MGR="apt"
        ;;
    *)
        warn "Unsupported distro '${DISTRO}' — attempting apt anyway"
        PKG_MGR="apt"
        ;;
esac

# ── Install dependencies ──────────────────────────────────────
install_deps() {
    step "Installing build dependencies via ${PKG_MGR}"

    local DEPS=(
        # Build tools
        build-essential gcc g++ cmake ninja-build pkg-config git

        # Qt6 development packages
        qt6-base-dev qt6-base-private-dev
        qt6-webengine-dev qt6-webengine-dev-tools
        qt6-webchannel-dev qt6-multimedia-dev
        qt6-declarative-dev
        libqt6opengl6-dev

        # System libraries
        libsqlite3-dev
        libgl1-mesa-dev libegl1-mesa-dev
        libdrm-dev libgbm-dev
        libfontconfig1-dev libfreetype6-dev
        libxkbcommon-dev libxkbcommon-x11-dev
        libx11-xcb-dev libxcb1-dev libxext-dev libxfixes-dev
        libxcomposite-dev libxdamage-dev libxrandr-dev libxtst-dev
        libnss3-dev libnspr4-dev
        libdbus-1-dev
        libglib2.0-dev
        libasound2-dev libpulse-dev
        libexpat1-dev

        # .NET 8 SDK (for C# NativeAOT core)
        # Handled separately below
    )

    sudo apt-get update
    # Install what's available — some packages may not exist on all versions
    sudo apt-get install -y "${DEPS[@]}" 2>/dev/null || {
        warn "Some packages unavailable — trying individually"
        for pkg in "${DEPS[@]}"; do
            sudo apt-get install -y "$pkg" 2>/dev/null || warn "Skipped: $pkg"
        done
    }

    # .NET SDK — try Microsoft repo first, then snap, then skip
    if ! command -v dotnet &>/dev/null; then
        step "Installing .NET 8 SDK"
        if [ -f /etc/apt/sources.list.d/microsoft-prod.list ] || \
           apt-cache show dotnet-sdk-8.0 &>/dev/null 2>&1; then
            sudo apt-get install -y dotnet-sdk-8.0 || true
        else
            info "Adding Microsoft package repository..."
            # Detect Ubuntu version for correct repo
            local UBUNTU_VER
            UBUNTU_VER=$(lsb_release -rs 2>/dev/null || echo "22.04")
            # For Mint, map to Ubuntu base
            if [ "$DISTRO" = "linuxmint" ]; then
                case "$UBUNTU_VER" in
                    21*)  UBUNTU_VER="22.04" ;;
                    22*)  UBUNTU_VER="24.04" ;;
                    *)    UBUNTU_VER="22.04" ;;
                esac
            fi
            wget -q "https://packages.microsoft.com/config/ubuntu/${UBUNTU_VER}/packages-microsoft-prod.deb" \
                -O /tmp/packages-microsoft-prod.deb 2>/dev/null && \
                sudo dpkg -i /tmp/packages-microsoft-prod.deb && \
                sudo apt-get update && \
                sudo apt-get install -y dotnet-sdk-8.0 || {
                    warn ".NET SDK install failed — C# core will be skipped"
                    warn "You can install manually: https://dotnet.microsoft.com/download"
                }
        fi
    fi

    ok "Dependencies installed"
}

# Deps-only mode
if [ "$ACTION" = "deps" ]; then
    install_deps
    exit 0
fi

# ── Check dependencies ────────────────────────────────────────
step "Checking build tools"

MISSING=()
for cmd in cmake g++ pkg-config; do
    if ! command -v "$cmd" &>/dev/null; then
        MISSING+=("$cmd")
    fi
done

# Check Qt6
QT6_FOUND=false
if pkg-config --exists Qt6Core 2>/dev/null; then
    QT6_FOUND=true
    QT6_VER=$(pkg-config --modversion Qt6Core 2>/dev/null || echo "unknown")
elif [ -n "${QT_PREFIX:-}" ] && [ -d "${QT_PREFIX}/lib" ]; then
    QT6_FOUND=true
    QT6_VER="(from QT_PREFIX)"
elif [ -n "${Qt6_DIR:-}" ]; then
    QT6_FOUND=true
    QT6_VER="(from Qt6_DIR)"
fi

if [ ${#MISSING[@]} -gt 0 ] || [ "$QT6_FOUND" = "false" ]; then
    warn "Missing tools: ${MISSING[*]:-none}"
    [ "$QT6_FOUND" = "false" ] && warn "Qt6 development packages not found"
    echo ""
    info "Install dependencies with: $0 --deps-only"
    echo ""
    read -rp "Install dependencies now? [y/N] " ans
    if [[ "$ans" =~ ^[Yy] ]]; then
        install_deps
    else
        err "Cannot continue without dependencies"
        exit 1
    fi
fi

ok "Build tools ready"
if [ "$QT6_FOUND" = "true" ]; then
    info "Qt6: ${QT6_VER}"
fi
if command -v dotnet &>/dev/null; then
    info ".NET: $(dotnet --version 2>/dev/null || echo 'available')"
else
    warn ".NET SDK not found — C# NativeAOT core will be skipped"
fi

# ── Resolve Qt prefix ────────────────────────────────────────
resolve_qt_prefix() {
    if [ -n "${QT_PREFIX:-}" ] && [ -d "${QT_PREFIX}/lib" ]; then
        return 0
    fi

    # Try QT_ROOT_DIR (set by jurplel/install-qt-action)
    if [ -n "${QT_ROOT_DIR:-}" ] && [ -d "${QT_ROOT_DIR}/lib" ]; then
        QT_PREFIX="${QT_ROOT_DIR}"
        return 0
    fi

    # Try Qt6_DIR (walk up from lib/cmake/Qt6)
    if [ -n "${Qt6_DIR:-}" ]; then
        local candidate
        candidate="$(cd "${Qt6_DIR}/../../.." 2>/dev/null && pwd)"
        if [ -d "${candidate}/lib" ]; then
            QT_PREFIX="${candidate}"
            return 0
        fi
    fi

    # Try system Qt6
    local qt6core_pc
    qt6core_pc=$(pkg-config --variable=libdir Qt6Core 2>/dev/null || true)
    if [ -n "$qt6core_pc" ]; then
        QT_PREFIX="$(dirname "$qt6core_pc")"
        return 0
    fi

    # Common install locations
    for loc in /usr /usr/lib/x86_64-linux-gnu/cmake/Qt6 /opt/qt6/6.8.3/gcc_64; do
        if [ -d "${loc}/lib" ] && ls "${loc}/lib"/libQt6Core.so* &>/dev/null 2>&1; then
            QT_PREFIX="$loc"
            return 0
        fi
    done

    # Use system default — cmake will find it
    QT_PREFIX=""
}
resolve_qt_prefix

CMAKE_PREFIX_ARG=""
if [ -n "${QT_PREFIX:-}" ]; then
    CMAKE_PREFIX_ARG="-DCMAKE_PREFIX_PATH=${QT_PREFIX}"
    info "Qt prefix: ${QT_PREFIX}"
fi

# ── Build ──────────────────────────────────────────────────────
step "Building OyNIx (${BUILD_TYPE}, ${JOBS} threads)"

# Prefer Ninja
CMAKE_GEN=""
BUILD_TOOL_ARGS=""
if command -v ninja &>/dev/null; then
    CMAKE_GEN="-GNinja"
    info "Using Ninja build system"
else
    info "Using Make build system"
fi

mkdir -p "${BUILD_DIR}"

cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    ${CMAKE_GEN} \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    ${CMAKE_PREFIX_ARG} \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "${BUILD_DIR}" -j"${JOBS}"

if [ ! -f "${BUILD_DIR}/oynix" ]; then
    err "Build failed — oynix binary not found"
    exit 1
fi

ok "Build complete: ${BUILD_DIR}/oynix"

# ── Size info ──────────────────────────────────────────────────
BINARY_SIZE=$(du -sh "${BUILD_DIR}/oynix" | awk '{print $1}')
info "Binary size: ${BINARY_SIZE}"

if [ -f "${BUILD_DIR}/dotnet/OyNIxCore.so" ]; then
    CORE_SIZE=$(du -sh "${BUILD_DIR}/dotnet/OyNIxCore.so" | awk '{print $1}')
    info "C# core: ${CORE_SIZE}"
fi

# ── Action: build only ─────────────────────────────────────────
if [ "$ACTION" = "build" ]; then
    echo ""
    echo -e "${GREEN}${BOLD}Build successful!${NC}"
    echo ""
    echo "  Binary:  ${BUILD_DIR}/oynix"
    echo "  Run:     ${BUILD_DIR}/oynix"
    echo ""
    echo "  Next steps:"
    echo "    $0 --install     Install to ~/.local (no sudo)"
    echo "    $0 --system      Install to /usr (sudo)"
    echo "    $0 --deb         Build .deb package"
    exit 0
fi

# ── Action: install to ~/.local ────────────────────────────────
if [ "$ACTION" = "install-local" ]; then
    step "Installing to ~/.local"

    LOCAL_DIR="${HOME}/.local"
    BIN_DIR="${LOCAL_DIR}/bin"
    LIB_DIR="${LOCAL_DIR}/lib/oynix"
    SHARE_DIR="${LOCAL_DIR}/share"
    APP_DIR="${SHARE_DIR}/applications"
    THEME_DIR="${SHARE_DIR}/oynix/themes"

    mkdir -p "${BIN_DIR}" "${LIB_DIR}" "${APP_DIR}" "${THEME_DIR}"

    # Binary
    install -m755 "${BUILD_DIR}/oynix" "${LIB_DIR}/oynix"

    # C# core
    if [ -f "${BUILD_DIR}/dotnet/OyNIxCore.so" ]; then
        install -m644 "${BUILD_DIR}/dotnet/OyNIxCore.so" "${LIB_DIR}/"
    fi

    # Themes
    if [ -d "${PROJECT_DIR}/themes" ]; then
        cp -r "${PROJECT_DIR}/themes"/* "${THEME_DIR}/" 2>/dev/null || true
    fi

    # Icons
    for size in 256 128 64 48; do
        icon="${PROJECT_DIR}/assets/icon-${size}.png"
        if [ -f "$icon" ]; then
            icon_dir="${SHARE_DIR}/icons/hicolor/${size}x${size}/apps"
            mkdir -p "$icon_dir"
            install -m644 "$icon" "${icon_dir}/oynix.png"
        fi
    done
    if [ -f "${PROJECT_DIR}/assets/icon.svg" ]; then
        mkdir -p "${SHARE_DIR}/icons/hicolor/scalable/apps"
        install -m644 "${PROJECT_DIR}/assets/icon.svg" \
            "${SHARE_DIR}/icons/hicolor/scalable/apps/oynix.svg"
    fi

    # CLI launcher
    cat > "${BIN_DIR}/oynix" <<LAUNCHER
#!/bin/bash
# OyNIx Browser — CLI launcher
OYNIX_DIR="${LIB_DIR}"
export LD_LIBRARY_PATH="\${OYNIX_DIR}\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
exec "\${OYNIX_DIR}/oynix" "\$@"
LAUNCHER
    chmod 755 "${BIN_DIR}/oynix"

    # Desktop entry
    cat > "${APP_DIR}/oynix.desktop" <<DESKTOP
[Desktop Entry]
Name=OyNIx Browser
Comment=Chromium-based desktop browser with local AI
Exec=${BIN_DIR}/oynix %u
Icon=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Keywords=browser;web;internet;ai;nyx;
StartupNotify=true
StartupWMClass=OyNIx Browser
DESKTOP

    # Update desktop database
    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "${APP_DIR}" 2>/dev/null || true
    fi

    ok "Installed to ${LOCAL_DIR}"
    echo ""
    echo -e "${GREEN}${BOLD}Installation complete!${NC}"
    echo ""
    echo "  Run from terminal:  oynix"
    echo "  Or find it in your application menu."
    echo ""
    if [[ ":$PATH:" != *":${BIN_DIR}:"* ]]; then
        warn "${BIN_DIR} is not in your PATH"
        echo "  Add it:  export PATH=\"${BIN_DIR}:\$PATH\""
        echo "  Or add to ~/.bashrc:  echo 'export PATH=\"\${HOME}/.local/bin:\$PATH\"' >> ~/.bashrc"
    fi
    exit 0
fi

# ── Action: install to /usr ────────────────────────────────────
if [ "$ACTION" = "install-system" ]; then
    step "Installing to /usr (requires sudo)"

    cd "${BUILD_DIR}"
    sudo cmake --install .

    # Create CLI launcher
    sudo tee /usr/bin/oynix > /dev/null <<'LAUNCHER'
#!/bin/bash
# OyNIx Browser — CLI launcher
OYNIX_DIR="/usr/lib/oynix"
export LD_LIBRARY_PATH="${OYNIX_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "${OYNIX_DIR}/oynix" "$@"
LAUNCHER
    sudo chmod 755 /usr/bin/oynix

    # Update caches
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true

    ok "Installed to /usr"
    echo ""
    echo -e "${GREEN}${BOLD}System installation complete!${NC}"
    echo ""
    echo "  Run:  oynix"
    echo "  Uninstall:  sudo rm -rf /usr/lib/oynix /usr/bin/oynix"
    exit 0
fi

# ── Action: build .deb ─────────────────────────────────────────
if [ "$ACTION" = "deb" ]; then
    step "Building .deb package"

    if ! command -v dpkg-deb &>/dev/null; then
        err "dpkg-deb not found — install with: sudo apt install dpkg-dev"
        exit 1
    fi

    # Delegate to the full deb builder
    if [ -x "${PROJECT_DIR}/scripts/build-deb.sh" ]; then
        exec "${PROJECT_DIR}/scripts/build-deb.sh" "3.1.0"
    else
        err "scripts/build-deb.sh not found"
        exit 1
    fi
fi
