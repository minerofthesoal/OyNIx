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
# Set LD_LIBRARY_PATH for pip-installed Qt6 WebEngine libraries
QT6_LIB=$(python3 -c "import PyQt6,os; print(os.path.join(os.path.dirname(PyQt6.__file__),'Qt6','lib'))" 2>/dev/null)
if [ -n "$QT6_LIB" ] && [ -d "$QT6_LIB" ]; then
    export LD_LIBRARY_PATH="${QT6_LIB}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export PYTHONPATH="/usr/lib/oynix${PYTHONPATH:+:$PYTHONPATH}"
cd /usr/lib/oynix
exec python3 -m oynix "$@"
LAUNCHER
fi
chmod +x "${PKG}/usr/bin/oynix"

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

# Build turbo indexer if g++ available
if command -v g++ &>/dev/null && [ -f "src/native/turbo_index.cpp" ]; then
    echo "  Building turbo indexer..."
    g++ -O3 -shared -fPIC -std=c++17 -pthread \
        -o "${PKG}/usr/lib/oynix/build/libturbo_index.so" \
        src/native/turbo_index.cpp 2>/dev/null || echo "  (turbo indexer build skipped)"
fi

# Desktop entry
cat > "${PKG}/usr/share/applications/oynix.desktop" <<'DESKTOP'
[Desktop Entry]
Name=OyNIx Browser
Comment=Nyx-Powered Local AI Browser
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
Description: Nyx-themed desktop browser with local AI assistant
 Custom Firefox-inspired browser with tree tabs, local LLM,
 auto-indexing Nyx search engine, and purple/black theme.
 Coded by Claude (Anthropic).
Depends: python3 (>= 3.10), python3-pip, python3-venv, libegl1, libgl1, libxkbcommon0, libnss3, libxcomposite1, libxdamage1, libxrandr2, libxtst6, libasound2
Recommends: libqt6webchannel6, libqt6webenginecore6, libqt6webenginewidgets6, g++, gcc
CTRL

# Post-install: install Python deps
cat > "${PKG}/DEBIAN/postinst" <<'POSTINST'
#!/bin/bash
echo "Installing OyNIx Python dependencies..."

# Try venv first, then --break-system-packages, then plain pip
if python3 -m pip install --break-system-packages -r /usr/lib/oynix/requirements.txt 2>/dev/null; then
    echo "  Dependencies installed (pip --break-system-packages)"
elif python3 -m pip install -r /usr/lib/oynix/requirements.txt 2>/dev/null; then
    echo "  Dependencies installed (pip)"
else
    echo ""
    echo "WARNING: Could not auto-install Python deps."
    echo "Run manually: pip install --break-system-packages -r /usr/lib/oynix/requirements.txt"
    echo "Or use a venv: python3 -m venv /tmp/oynix-venv && source /tmp/oynix-venv/bin/activate && pip install -r /usr/lib/oynix/requirements.txt"
fi

# Verify critical deps
if python3 -c "from PyQt6.QtWidgets import QApplication" 2>/dev/null; then
    echo "  PyQt6: OK"
else
    echo "  WARNING: PyQt6 not working. Run: pip install --break-system-packages PyQt6 PyQt6-WebEngine"
fi
POSTINST
chmod +x "${PKG}/DEBIAN/postinst"

# Build .deb
dpkg-deb --root-owner-group --build "${PKG}"
mv "${PKG}.deb" "oynix-${VERSION}-${ARCH}.deb"
rm -rf "${PKG}"

echo "Done: oynix-${VERSION}-${ARCH}.deb"
