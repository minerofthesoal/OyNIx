#!/bin/bash
# ============================================================
#  OyNIx Browser - Build .deb Package
#  Usage: ./scripts/build-deb.sh [version]
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"  # Strip leading v
ARCH="amd64"
PKG="oynix_${VERSION}_${ARCH}"

echo "Building .deb package v${VERSION}..."

# Clean
rm -rf "${PKG}" "${PKG}.deb"

# Create directory structure
mkdir -p "${PKG}/DEBIAN"
mkdir -p "${PKG}/usr/lib/oynix/oynix"
mkdir -p "${PKG}/usr/lib/oynix/build"
mkdir -p "${PKG}/usr/bin"
mkdir -p "${PKG}/usr/share/applications"

# Copy Python source
cp -r oynix/* "${PKG}/usr/lib/oynix/oynix/"
cp requirements.txt "${PKG}/usr/lib/oynix/"

# Build and copy native library if possible
if command -v g++ &>/dev/null; then
    echo "  Building native C++ library..."
    g++ -O2 -shared -fPIC -std=c++17 \
        -o "${PKG}/usr/lib/oynix/build/libnyx_index.so" \
        src/native/fast_index.cpp 2>/dev/null || echo "  (native lib build skipped)"
fi

# Build and copy C launcher if possible
if command -v gcc &>/dev/null; then
    echo "  Building native C launcher..."
    gcc -O2 -o "${PKG}/usr/bin/oynix" src/launcher.c 2>/dev/null
fi

# Fallback shell launcher (in case C launcher failed)
if [ ! -f "${PKG}/usr/bin/oynix" ]; then
    cat > "${PKG}/usr/bin/oynix" <<'LAUNCHER'
#!/bin/bash
cd /usr/lib/oynix
exec python3 -m oynix "$@"
LAUNCHER
fi
chmod +x "${PKG}/usr/bin/oynix"

# Desktop entry
cat > "${PKG}/usr/share/applications/oynix.desktop" <<'DESKTOP'
[Desktop Entry]
Name=OyNIx Browser
Comment=Nyx-themed browser with local AI
Exec=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
DESKTOP

# Control file
cat > "${PKG}/DEBIAN/control" <<CTRL
Package: oynix
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: OyNIx Project
Description: Nyx-themed desktop browser with local AI assistant
 Custom Firefox-inspired browser with tree tabs, local LLM,
 auto-indexing Nyx search engine, and purple/black theme.
 Coded by Claude (Anthropic).
Depends: python3 (>= 3.10), python3-pip, python3-venv, libegl1, libgl1, libxkbcommon0, libnss3, libxcomposite1, libxdamage1, libxrandr2
Recommends: g++, gcc
CTRL

# Post-install: install Python deps
cat > "${PKG}/DEBIAN/postinst" <<'POSTINST'
#!/bin/bash
set -e
echo "Installing OyNIx Python dependencies..."
python3 -m pip install --break-system-packages -r /usr/lib/oynix/requirements.txt 2>/dev/null || \
python3 -m pip install -r /usr/lib/oynix/requirements.txt 2>/dev/null || \
echo "WARNING: Could not auto-install Python deps. Run: pip install -r /usr/lib/oynix/requirements.txt"
POSTINST
chmod +x "${PKG}/DEBIAN/postinst"

# Build .deb
dpkg-deb --root-owner-group --build "${PKG}"
mv "${PKG}.deb" "oynix-${VERSION}-${ARCH}.deb"
rm -rf "${PKG}"

echo "Done: oynix-${VERSION}-${ARCH}.deb"
