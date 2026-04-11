#!/bin/bash
# ============================================================
#  OyNIx Browser - Build Arch Linux pacman Package (C++ / NativeAOT)
#  Usage: ./scripts/build-pacman.sh [version]
#
#  Requires: base-devel, cmake, qt6-webengine, dotnet-sdk-8.0
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"
# Sanitize: pacman pkgver only allows alphanumerics and periods
PKGVER=$(echo "$VERSION" | tr -c '[:alnum:].\n' '.')
PKGVER="${PKGVER%.}"  # strip trailing dot
PKGVER="${PKGVER:=1.1.0}"  # fallback if empty

echo "Building pacman package v${PKGVER} (C++ build)..."

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILDDIR="$(mktemp -d)"
trap 'rm -rf "$BUILDDIR"' EXIT

# Copy source to build dir
cp -r "${SRCDIR}/src" "${BUILDDIR}/"
cp -r "${SRCDIR}/core" "${BUILDDIR}/"
cp -r "${SRCDIR}/themes" "${BUILDDIR}/" 2>/dev/null || true
cp -r "${SRCDIR}/resources" "${BUILDDIR}/" 2>/dev/null || true
cp -r "${SRCDIR}/assets" "${BUILDDIR}/" 2>/dev/null || true
cp "${SRCDIR}/CMakeLists.txt" "${BUILDDIR}/"

cat > "${BUILDDIR}/PKGBUILD" <<PKGEOF
pkgname=oynix
pkgver=${PKGVER}
pkgrel=1
pkgdesc="Chromium-based desktop browser with local AI assistant"
arch=('x86_64')
depends=('qt6-webengine' 'qt6-webchannel' 'nss' 'libxcomposite' 'libxrandr')
makedepends=('cmake' 'gcc' 'sqlite')
optdepends=('dotnet-sdk-8.0: rebuild C# NativeAOT core')

build() {
  cd "\${startdir}"
  cmake -S . -B _build \\
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
    install -Dm755 _build/dotnet/OyNIxCore.so "\${pkgdir}/usr/lib/oynix/OyNIxCore.so"
  fi

  # Themes
  if [ -d "themes" ]; then
    install -dm755 "\${pkgdir}/usr/share/oynix/themes"
    cp -r themes/* "\${pkgdir}/usr/share/oynix/themes/"
  fi

  # Icons
  for size in 256 128 64 48; do
    icon="assets/icon-\${size}.png"
    if [ -f "\$icon" ]; then
      install -Dm644 "\$icon" "\${pkgdir}/usr/share/icons/hicolor/\${size}x\${size}/apps/oynix.png"
    fi
  done

  # Desktop entry
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
}
PKGEOF

# Build
cd "${BUILDDIR}"
if [ "$(id -u)" = "0" ]; then
    useradd -m _builder 2>/dev/null || true
    chown -R _builder:_builder .
    su _builder -c "makepkg -sf --noconfirm --skipinteg --nodeps"
else
    makepkg -sf --noconfirm --skipinteg --nodeps
fi

# Copy result back
cp "${BUILDDIR}/"*.pkg.tar.* "${SRCDIR}/" 2>/dev/null
echo "Done: $(ls "${SRCDIR}/"*.pkg.tar.* 2>/dev/null)"
