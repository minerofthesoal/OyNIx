#!/bin/bash
# ============================================================
#  OyNIx Browser - Build .deb Package (C++ / NativeAOT)
#  Usage: ./scripts/build-deb.sh [version]
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"  # Strip leading v
ARCH="amd64"
PKG="oynix_${VERSION}_${ARCH}"

echo "Building .deb package v${VERSION} (C++ build)..."

# ── Prerequisites ──────────────────────────────────────────────
QT_PREFIX="${QT_PREFIX:-/opt/qt6/6.8.3/gcc_64}"
DOTNET="${DOTNET_ROOT:-/opt/dotnet}/dotnet"
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found" >&2; exit 1
fi

# ── Build with CMake ───────────────────────────────────────────
BUILD="_build_deb"
rm -rf "${BUILD}"
mkdir -p "${BUILD}"

cmake -S . -B "${BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "${BUILD}" -j"$(nproc)"

# ── Assemble .deb tree ─────────────────────────────────────────
rm -rf "${PKG}" "${PKG}.deb"
mkdir -p "${PKG}/DEBIAN"
mkdir -p "${PKG}/usr/bin"
mkdir -p "${PKG}/usr/lib/oynix"
mkdir -p "${PKG}/usr/share/applications"
mkdir -p "${PKG}/usr/share/oynix/themes"

# Binary
cp "${BUILD}/oynix" "${PKG}/usr/bin/oynix"
chmod +x "${PKG}/usr/bin/oynix"

# C# NativeAOT core library
if [ -f "${BUILD}/dotnet/OyNIxCore.so" ]; then
    cp "${BUILD}/dotnet/OyNIxCore.so" "${PKG}/usr/lib/oynix/"
fi

# Themes
if [ -d "themes" ]; then
    cp -r themes/* "${PKG}/usr/share/oynix/themes/" 2>/dev/null || true
fi

# Icons
if [ -d "assets" ]; then
    for size in 256 128 64 48; do
        icon="assets/icon-${size}.png"
        if [ -f "$icon" ]; then
            mkdir -p "${PKG}/usr/share/icons/hicolor/${size}x${size}/apps"
            cp "$icon" "${PKG}/usr/share/icons/hicolor/${size}x${size}/apps/oynix.png"
        fi
    done
fi

# Desktop entry
cat > "${PKG}/usr/share/applications/oynix.desktop" <<'DESKTOP'
[Desktop Entry]
Name=OyNIx Browser
Comment=Chromium-based desktop browser with local AI
Exec=oynix %u
Icon=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Keywords=browser;web;internet;ai;nyx;
StartupNotify=true
StartupWMClass=OyNIx Browser
DESKTOP

# Control file
cat > "${PKG}/DEBIAN/control" <<CTRL
Package: oynix
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: OyNIx Project
Description: Chromium-based desktop browser with local AI assistant
 Custom browser built with Qt6 WebEngine, C++17, and .NET 8
 NativeAOT core. Features tree tabs, local LLM, web crawler,
 auto-indexing search engine, and obsidian theme.
Depends: libegl1, libgl1, libxkbcommon0, libnss3, libxcomposite1, libxdamage1, libxrandr2, libxtst6, libasound2
Recommends: libqt6webchannel6, libqt6webenginecore6, libqt6webenginewidgets6
CTRL

# Build .deb
dpkg-deb --root-owner-group --build "${PKG}"
mv "${PKG}.deb" "oynix-${VERSION}-${ARCH}.deb"
rm -rf "${PKG}" "${BUILD}"

echo "Done: oynix-${VERSION}-${ARCH}.deb"
