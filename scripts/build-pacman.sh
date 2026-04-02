#!/bin/bash
# ============================================================
#  OyNIx Browser - Build Arch Linux pacman Package
#  Usage: ./scripts/build-pacman.sh [version]
#
#  Requires: base-devel, python, gcc (install before running)
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"
# Sanitize: pacman pkgver only allows alphanumerics and periods
PKGVER=$(echo "$VERSION" | tr -c '[:alnum:].\n' '.')
PKGVER="${PKGVER%.}"  # strip trailing dot
PKGVER="${PKGVER:=1.1.0}"  # fallback if empty

echo "Building pacman package v${PKGVER}..."

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILDDIR="$(mktemp -d)"
trap 'rm -rf "$BUILDDIR"' EXIT

# Copy source to build dir
cp -r "${SRCDIR}/oynix" "${BUILDDIR}/"
cp "${SRCDIR}/requirements.txt" "${BUILDDIR}/"
cp -r "${SRCDIR}/src" "${BUILDDIR}/" 2>/dev/null || true

# Create PKGBUILD — note: no depends that aren't on the build system
# We use makedepends for build-time, and the actual package will
# list runtime depends but we skip dep checks during build
cat > "${BUILDDIR}/PKGBUILD" <<PKGEOF
pkgname=oynix
pkgver=${PKGVER}
pkgrel=1
pkgdesc="Nyx-themed desktop browser with local AI assistant"
arch=('x86_64')
depends=()
makedepends=()
optdepends=('python-pyqt6: Qt6 bindings from pacman')

package() {
  # Install Python package
  install -dm755 "\${pkgdir}/usr/lib/oynix"
  cp -r "\${startdir}/oynix" "\${pkgdir}/usr/lib/oynix/"
  cp "\${startdir}/requirements.txt" "\${pkgdir}/usr/lib/oynix/"

  # Build native lib if g++ available
  if command -v g++ &>/dev/null && [ -f "\${startdir}/src/native/fast_index.cpp" ]; then
    mkdir -p "\${pkgdir}/usr/lib/oynix/build"
    g++ -O2 -shared -fPIC -std=c++17 \\
      -o "\${pkgdir}/usr/lib/oynix/build/libnyx_index.so" \\
      "\${startdir}/src/native/fast_index.cpp" 2>/dev/null || true
  fi

  # Build native launcher if gcc available
  install -dm755 "\${pkgdir}/usr/bin"
  if command -v gcc &>/dev/null && [ -f "\${startdir}/src/launcher.c" ]; then
    gcc -O2 -o "\${pkgdir}/usr/bin/oynix" "\${startdir}/src/launcher.c" 2>/dev/null || true
  fi

  # Fallback shell launcher
  if [ ! -f "\${pkgdir}/usr/bin/oynix" ]; then
    cat > "\${pkgdir}/usr/bin/oynix" <<'SH'
#!/bin/bash
# Set LD_LIBRARY_PATH for pip-installed Qt6 WebEngine libraries
QT6_LIB=\$(python3 -c "import PyQt6,os; print(os.path.join(os.path.dirname(PyQt6.__file__),'Qt6','lib'))" 2>/dev/null)
if [ -n "\$QT6_LIB" ] && [ -d "\$QT6_LIB" ]; then
    export LD_LIBRARY_PATH="\${QT6_LIB}\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
fi
export PYTHONPATH="/usr/lib/oynix\${PYTHONPATH:+:\$PYTHONPATH}"
cd /usr/lib/oynix
exec python3 -m oynix "\$@"
SH
  fi
  chmod +x "\${pkgdir}/usr/bin/oynix"

  # Desktop entry
  install -dm755 "\${pkgdir}/usr/share/applications"
  cat > "\${pkgdir}/usr/share/applications/oynix.desktop" <<'DESKTOP'
[Desktop Entry]
Name=OyNIx Browser
Comment=Nyx-themed browser with local AI
Exec=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
DESKTOP

  # Install script — runs pip install on user's machine
  install -dm755 "\${pkgdir}/usr/lib/oynix/scripts"
  cat > "\${pkgdir}/usr/lib/oynix/scripts/post-install.sh" <<'POST'
#!/bin/bash
echo "Installing OyNIx Python dependencies..."
pip install --break-system-packages -r /usr/lib/oynix/requirements.txt 2>/dev/null || \
pip install -r /usr/lib/oynix/requirements.txt 2>/dev/null || \
echo "Run: pip install -r /usr/lib/oynix/requirements.txt"
POST
  chmod +x "\${pkgdir}/usr/lib/oynix/scripts/post-install.sh"
}
PKGEOF

# Build — use --nodeps to skip dependency resolution in CI
cd "${BUILDDIR}"
if [ "$(id -u)" = "0" ]; then
    # makepkg refuses to run as root, create a builder user
    useradd -m _builder 2>/dev/null || true
    chown -R _builder:_builder .
    su _builder -c "makepkg -sf --noconfirm --skipinteg --nodeps"
else
    makepkg -sf --noconfirm --skipinteg --nodeps
fi

# Copy result back
cp "${BUILDDIR}/"*.pkg.tar.* "${SRCDIR}/" 2>/dev/null
echo "Done: $(ls "${SRCDIR}/"*.pkg.tar.* 2>/dev/null)"
