#!/bin/bash
# ============================================================
#  OyNIx Browser - Build Arch Linux pacman Package (C++ / NativeAOT)
#
#  Usage: ./scripts/build-pacman.sh [version]
#
#  Requires: base-devel cmake qt6-webengine qt6-webchannel sqlite
#  Optional: dotnet-sdk (for C# NativeAOT core)
# ============================================================
set -euo pipefail

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"
# Sanitize: pacman pkgver only allows alphanumerics and periods
PKGVER=$(echo "$VERSION" | tr -c '[:alnum:].\n' '.')
PKGVER="${PKGVER%.}"
PKGVER="${PKGVER:=1.1.0}"

echo "=== OyNIx pacman builder v${PKGVER} ==="

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILDDIR="$(mktemp -d)"
trap 'rm -rf "$BUILDDIR"' EXIT

echo "  Source: ${SRCDIR}"
echo "  Build:  ${BUILDDIR}"

# Copy full source tree needed for the build
cp -r "${SRCDIR}/src" "${BUILDDIR}/"
cp -r "${SRCDIR}/core" "${BUILDDIR}/"
cp "${SRCDIR}/CMakeLists.txt" "${BUILDDIR}/"
cp "${SRCDIR}/global.json" "${BUILDDIR}/" 2>/dev/null || true
cp -r "${SRCDIR}/themes" "${BUILDDIR}/" 2>/dev/null || true
cp -r "${SRCDIR}/resources" "${BUILDDIR}/" 2>/dev/null || true
cp -r "${SRCDIR}/assets" "${BUILDDIR}/" 2>/dev/null || true

# ── Generate PKGBUILD ─────────────────────────────────────────
cat > "${BUILDDIR}/PKGBUILD" <<PKGEOF
# Maintainer: OyNIx Project
pkgname=oynix
pkgver=${PKGVER}
pkgrel=1
pkgdesc="Chromium-based desktop browser with local AI assistant"
arch=('x86_64')
url="https://github.com/minerofthesoal/OyNIx"
license=('MIT')
depends=(
    'qt6-webengine'
    'qt6-webchannel'
    'nss'
    'nspr'
    'libxcomposite'
    'libxrandr'
    'libxtst'
    'libxkbcommon'
    'libdrm'
    'mesa'
)
makedepends=('cmake' 'gcc' 'sqlite' 'ninja')
optdepends=('dotnet-sdk: rebuild C# NativeAOT core')

build() {
    cd "\${startdir}"

    local _generator=""
    if command -v ninja &>/dev/null; then
        _generator="-GNinja"
    fi

    cmake -S . -B _build \\
        \${_generator} \\
        -DCMAKE_BUILD_TYPE=Release \\
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build _build -j\$(nproc)
}

package() {
    cd "\${startdir}"

    # Binary
    install -Dm755 _build/oynix "\${pkgdir}/usr/bin/oynix"

    # C# NativeAOT core
    if [ -f "_build/dotnet/OyNIxCore.so" ]; then
        install -Dm644 _build/dotnet/OyNIxCore.so \\
            "\${pkgdir}/usr/lib/oynix/OyNIxCore.so"
    fi

    # Themes
    if [ -d "themes" ]; then
        install -dm755 "\${pkgdir}/usr/share/oynix/themes"
        install -m644 themes/*.css "\${pkgdir}/usr/share/oynix/themes/"
    fi

    # Icons
    for size in 256 128 64 48; do
        icon="assets/icon-\${size}.png"
        if [ -f "\${icon}" ]; then
            install -Dm644 "\${icon}" \\
                "\${pkgdir}/usr/share/icons/hicolor/\${size}x\${size}/apps/oynix.png"
        fi
    done
    if [ -f "assets/icon.svg" ]; then
        install -Dm644 "assets/icon.svg" \\
            "\${pkgdir}/usr/share/icons/hicolor/scalable/apps/oynix.svg"
    fi

    # Desktop entry
    if [ -f "resources/oynix.desktop" ]; then
        install -Dm644 resources/oynix.desktop \\
            "\${pkgdir}/usr/share/applications/oynix.desktop"
    else
        install -dm755 "\${pkgdir}/usr/share/applications"
        cat > "\${pkgdir}/usr/share/applications/oynix.desktop" <<'DESKTOP'
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
    fi
}
PKGEOF

# ── Build ─────────────────────────────────────────────────────
cd "${BUILDDIR}"

if [ "$(id -u)" = "0" ]; then
    # makepkg refuses to run as root — create a build user
    if ! id -u _builder &>/dev/null 2>&1; then
        useradd -m _builder 2>/dev/null || true
    fi
    chown -R _builder:_builder .
    su _builder -c "makepkg -sf --noconfirm --skipinteg --nodeps"
else
    makepkg -sf --noconfirm --skipinteg --nodeps
fi

# ── Copy result back ──────────────────────────────────────────
PKG_FILES=$(ls "${BUILDDIR}/"*.pkg.tar.* 2>/dev/null || true)
if [ -z "$PKG_FILES" ]; then
    echo "ERROR: No .pkg.tar.* files produced" >&2
    exit 1
fi

cp ${PKG_FILES} "${SRCDIR}/"

echo ""
echo "=== Done ==="
for f in ${PKG_FILES}; do
    base=$(basename "$f")
    size=$(du -sh "${SRCDIR}/${base}" | awk '{print $1}')
    echo "  File: ${base} (${size})"
done
echo "  Install: sudo pacman -U oynix-*.pkg.tar.zst"
