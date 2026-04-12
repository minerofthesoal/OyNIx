#!/bin/bash
# ============================================================
#  OyNIx Browser - Build .deb Package (C++ / NativeAOT)
#  Produces a proper apt-installable .deb with bundled Qt6.
#  Usage: QT_PREFIX=/path/to/qt ./scripts/build-deb.sh [version]
# ============================================================
set -e

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"  # Strip leading v
# Sanitize version for dpkg (only allow digits, letters, dots, hyphens, tildes, plus)
VERSION=$(echo "$VERSION" | tr -c '[:alnum:].~+\n-' '.' | sed 's/\.$//')
VERSION="${VERSION:-1.1.0}"
ARCH="amd64"
PKG="oynix_${VERSION}_${ARCH}"

echo "Building .deb package v${VERSION} (C++ build)..."

# ── Prerequisites ──────────────────────────────────────────────
QT_PREFIX="${QT_PREFIX:-/opt/qt6/6.8.3/gcc_64}"
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found" >&2; exit 1
fi
if ! command -v dpkg-deb &>/dev/null; then
    echo "ERROR: dpkg-deb not found (install dpkg-dev)" >&2; exit 1
fi

echo "Using Qt from: ${QT_PREFIX}"

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
LIB_DIR="${PKG}/usr/lib/oynix"

rm -rf "${PKG}" "${PKG}.deb"
mkdir -p "${PKG}/DEBIAN"
mkdir -p "${PKG}/usr/bin"
mkdir -p "${LIB_DIR}"
mkdir -p "${PKG}/usr/share/applications"
mkdir -p "${PKG}/usr/share/oynix/themes"
mkdir -p "${PKG}/usr/share/doc/oynix"

# ── Binary (installed to lib dir, wrapper in bin) ──────────────
cp "${BUILD}/oynix" "${LIB_DIR}/oynix"
chmod 755 "${LIB_DIR}/oynix"

# ── C# NativeAOT core library ─────────────────────────────────
if [ -f "${BUILD}/dotnet/OyNIxCore.so" ]; then
    cp "${BUILD}/dotnet/OyNIxCore.so" "${LIB_DIR}/"
    chmod 644 "${LIB_DIR}/OyNIxCore.so"
fi

# ── Bundle Qt6 libraries ──────────────────────────────────────
echo "Bundling Qt6 libraries..."
QT_LIB="${QT_PREFIX}/lib"

# Copy all Qt .so files the binary needs (use ldd to find them)
NEEDED_QT_LIBS=$(ldd "${LIB_DIR}/oynix" 2>/dev/null \
    | grep -oP "${QT_LIB}/\S+" | sort -u || true)

for lib in $NEEDED_QT_LIBS; do
    cp -L "$lib" "${LIB_DIR}/"
done

# Also copy libicu*.so* that Qt bundles (needed by QtCore)
for icu in "${QT_LIB}"/libicu*.so.*; do
    [ -f "$icu" ] || continue
    base=$(basename "$icu")
    [ -f "${LIB_DIR}/${base}" ] && continue
    cp -L "$icu" "${LIB_DIR}/"
done
# Deduplicate ICU: keep only the shortest-named version of each lib
for icu in "${LIB_DIR}"/libicu*.so.*.*; do
    [ -f "$icu" ] || continue
    short=$(echo "$icu" | sed 's/\.[0-9]*$//')
    if [ -f "$short" ]; then
        rm -f "$icu"
    fi
done

# Copy Qt plugins
for plugdir in platforms xcbglintegrations imageformats tls egldeviceintegrations wayland-shell-integration; do
    src="${QT_PREFIX}/plugins/${plugdir}"
    if [ -d "$src" ]; then
        mkdir -p "${LIB_DIR}/plugins/${plugdir}"
        cp -L "$src"/*.so "${LIB_DIR}/plugins/${plugdir}/" 2>/dev/null || true
    fi
done

# Copy QtWebEngineProcess and resources
if [ -d "${QT_PREFIX}/libexec" ]; then
    mkdir -p "${LIB_DIR}/libexec"
    for f in QtWebEngineProcess; do
        [ -f "${QT_PREFIX}/libexec/$f" ] && cp -L "${QT_PREFIX}/libexec/$f" "${LIB_DIR}/libexec/"
    done
fi

if [ -d "${QT_PREFIX}/resources" ]; then
    mkdir -p "${LIB_DIR}/resources"
    cp -L "${QT_PREFIX}/resources"/* "${LIB_DIR}/resources/" 2>/dev/null || true
fi

# Copy Qt translations
if [ -d "${QT_PREFIX}/translations" ]; then
    mkdir -p "${LIB_DIR}/translations"
    cp -L "${QT_PREFIX}/translations"/qtwebengine_locales/*.pak \
        "${LIB_DIR}/translations/" 2>/dev/null || true
fi

# Resolve transitive Qt library dependencies
echo "Resolving transitive Qt dependencies..."
MAX_ROUNDS=3
for round in $(seq 1 $MAX_ROUNDS); do
    MISSING=0
    for so in "${LIB_DIR}"/*.so* "${LIB_DIR}"/plugins/*/*.so* "${LIB_DIR}"/libexec/*; do
        [ -f "$so" ] || continue
        DEPS=$(ldd "$so" 2>/dev/null | grep "not found" | awk '{print $1}' || true)
        for dep in $DEPS; do
            SRC="${QT_LIB}/${dep}"
            if [ -f "$SRC" ] && [ ! -f "${LIB_DIR}/${dep}" ]; then
                cp -L "$SRC" "${LIB_DIR}/"
                MISSING=1
            fi
        done
    done
    [ "$MISSING" = "0" ] && break
done

echo "Bundled $(ls "${LIB_DIR}"/*.so* 2>/dev/null | wc -l) library files"

# ── Set permissions on bundled libraries ──────────────────────
find "${LIB_DIR}" -name '*.so*' -exec chmod 644 {} +
find "${LIB_DIR}/libexec" -type f -exec chmod 755 {} + 2>/dev/null || true

# ── Launcher wrapper ──────────────────────────────────────────
cat > "${PKG}/usr/bin/oynix" <<'LAUNCHER'
#!/bin/bash
# OyNIx Browser launcher — sets up bundled Qt environment
OYNIX_DIR="/usr/lib/oynix"
export LD_LIBRARY_PATH="${OYNIX_DIR}:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${OYNIX_DIR}/plugins"
export QTWEBENGINEPROCESS_PATH="${OYNIX_DIR}/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="${OYNIX_DIR}/resources"
export QTWEBENGINE_LOCALES_PATH="${OYNIX_DIR}/translations"
exec "${OYNIX_DIR}/oynix" "$@"
LAUNCHER
chmod 755 "${PKG}/usr/bin/oynix"

# ── Themes ────────────────────────────────────────────────────
if [ -d "themes" ]; then
    cp -r themes/* "${PKG}/usr/share/oynix/themes/" 2>/dev/null || true
fi

# ── Icons ─────────────────────────────────────────────────────
if [ -d "assets" ]; then
    for size in 256 128 64 48; do
        icon="assets/icon-${size}.png"
        if [ -f "$icon" ]; then
            mkdir -p "${PKG}/usr/share/icons/hicolor/${size}x${size}/apps"
            cp "$icon" "${PKG}/usr/share/icons/hicolor/${size}x${size}/apps/oynix.png"
        fi
    done
fi

# ── Desktop entry ─────────────────────────────────────────────
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

# ── Copyright / doc ───────────────────────────────────────────
cat > "${PKG}/usr/share/doc/oynix/copyright" <<'COPYRIGHT'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: OyNIx Browser
License: MIT

Files: *
Copyright: 2026 OyNIx Project
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files, to deal in the Software
 without restriction.
COPYRIGHT

# ── postinst — update desktop database and icon cache ─────────
cat > "${PKG}/DEBIAN/postinst" <<'POSTINST'
#!/bin/bash
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    for dir in /usr/share/icons/hicolor; do
        [ -d "$dir" ] && gtk-update-icon-cache -q "$dir" || true
    done
fi
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database /usr/share/mime || true
fi
POSTINST
chmod 755 "${PKG}/DEBIAN/postinst"

# ── postrm — cleanup on removal ──────────────────────────────
cat > "${PKG}/DEBIAN/postrm" <<'POSTRM'
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        for dir in /usr/share/icons/hicolor; do
            [ -d "$dir" ] && gtk-update-icon-cache -q "$dir" || true
        done
    fi
fi
if [ "$1" = "purge" ]; then
    rm -rf /usr/share/oynix 2>/dev/null || true
fi
POSTRM
chmod 755 "${PKG}/DEBIAN/postrm"

# ── DEBIAN/control ────────────────────────────────────────────
INSTALLED_SIZE=$(du -sk "${PKG}" | awk '{print $1}')

cat > "${PKG}/DEBIAN/control" <<CTRL
Package: oynix
Version: ${VERSION}
Section: web
Priority: optional
Architecture: ${ARCH}
Maintainer: OyNIx Project <oynix@users.noreply.github.com>
Homepage: https://github.com/minerofthesoal/OyNIx
Installed-Size: ${INSTALLED_SIZE}
Description: Chromium-based desktop browser with local AI assistant
 OyNIx is a custom browser built with Qt6 WebEngine, C++17, and
 .NET 8 NativeAOT core. Features include tree tabs, Nyx local search
 engine, web crawler, auto-indexing, and an obsidian theme.
 .
 Qt6 libraries are bundled — no external Qt packages required.
 Install with: sudo apt install ./oynix-*.deb
Depends: libegl1, libgl1, libglib2.0-0, libfontconfig1, libfreetype6,
 libxkbcommon0, libnss3, libnspr4, libxcomposite1, libxdamage1,
 libxrandr2, libxtst6, libx11-6, libx11-xcb1, libxcb1, libxext6,
 libxfixes3, libdbus-1-3, libasound2 | libasound2t64, libpulse0, libdrm2,
 libgbm1, libexpat1
CTRL

# ── Build .deb ────────────────────────────────────────────────
# Fix ownership for all files (dpkg-deb requires root ownership)
dpkg-deb --root-owner-group --build "${PKG}"
mv "${PKG}.deb" "oynix-${VERSION}-${ARCH}.deb"

# Verify the package
echo ""
echo "=== Package verification ==="
dpkg-deb --info "oynix-${VERSION}-${ARCH}.deb"
echo ""
dpkg-deb --contents "oynix-${VERSION}-${ARCH}.deb" | head -30
echo "..."

DEB_SIZE=$(du -sh "oynix-${VERSION}-${ARCH}.deb" | awk '{print $1}')
echo ""
echo "Done: oynix-${VERSION}-${ARCH}.deb (${DEB_SIZE})"
echo "Install with: sudo apt install ./oynix-${VERSION}-${ARCH}.deb"

rm -rf "${PKG}" "${BUILD}"
