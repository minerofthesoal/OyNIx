#!/bin/bash
# ============================================================
#  OyNIx Browser - Build Arch Linux pacman Package
#  Usage: ./scripts/build-pacman.sh [version]
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"
# Sanitize: pacman pkgver only allows alphanumerics and periods
PKGVER=$(echo "$VERSION" | tr -c '[:alnum:].' '.')
PKGVER="${PKGVER%.}"  # strip trailing dot

BUILDDIR="$(mktemp -d)"
trap "rm -rf $BUILDDIR" EXIT

echo "Building pacman package v${PKGVER}..."

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"

# Copy source to build dir
cp -r "${SRCDIR}/oynix" "${BUILDDIR}/"
cp "${SRCDIR}/requirements.txt" "${BUILDDIR}/"
cp -r "${SRCDIR}/src" "${BUILDDIR}/" 2>/dev/null || true

# Create PKGBUILD
cat > "${BUILDDIR}/PKGBUILD" <<EOF
pkgname=oynix
pkgver=${PKGVER}
pkgrel=1
pkgdesc="Nyx-themed desktop browser with local AI assistant"
arch=('x86_64')
depends=('python' 'python-pip')
optdepends=('gcc: native launcher' 'python-pyqt6: Qt6 bindings')

package() {
  # Install Python package
  install -dm755 "\$pkgdir/usr/lib/oynix"
  cp -r "\$startdir/oynix" "\$pkgdir/usr/lib/oynix/"
  cp "\$startdir/requirements.txt" "\$pkgdir/usr/lib/oynix/"

  # Build native lib if g++ available
  if command -v g++ &>/dev/null && [ -f "\$startdir/src/native/fast_index.cpp" ]; then
    mkdir -p "\$pkgdir/usr/lib/oynix/build"
    g++ -O2 -shared -fPIC -std=c++17 \\
      -o "\$pkgdir/usr/lib/oynix/build/libnyx_index.so" \\
      "\$startdir/src/native/fast_index.cpp" 2>/dev/null || true
  fi

  # Build native launcher if gcc available
  install -dm755 "\$pkgdir/usr/bin"
  if command -v gcc &>/dev/null && [ -f "\$startdir/src/launcher.c" ]; then
    gcc -O2 -o "\$pkgdir/usr/bin/oynix" "\$startdir/src/launcher.c" 2>/dev/null
  fi

  # Fallback shell launcher
  if [ ! -f "\$pkgdir/usr/bin/oynix" ]; then
    printf '#!/bin/bash\ncd /usr/lib/oynix\nexec python3 -m oynix "\$@"\n' \\
      > "\$pkgdir/usr/bin/oynix"
  fi
  chmod +x "\$pkgdir/usr/bin/oynix"

  # Desktop entry
  install -dm755 "\$pkgdir/usr/share/applications"
  cat > "\$pkgdir/usr/share/applications/oynix.desktop" <<'DESKTOP'
[Desktop Entry]
Name=OyNIx Browser
Comment=Nyx-themed browser with local AI
Exec=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
DESKTOP
}
EOF

# Copy source files needed by PKGBUILD
cp -r "${SRCDIR}/src" "${BUILDDIR}/" 2>/dev/null || true

# Build
cd "${BUILDDIR}"
if [ "$(id -u)" = "0" ]; then
    useradd -m _builder 2>/dev/null || true
    chown -R _builder:_builder .
    su _builder -c "makepkg -sf --noconfirm --skipinteg"
else
    makepkg -sf --noconfirm --skipinteg
fi

# Copy result back
cp "${BUILDDIR}/"*.pkg.tar.* "${SRCDIR}/" 2>/dev/null || true
echo "Done: $(ls "${SRCDIR}/"*.pkg.tar.* 2>/dev/null)"
