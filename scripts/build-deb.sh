#!/bin/bash
# ============================================================
#  OyNIx Browser - Build .deb Package (C++ / NativeAOT)
#  Produces a proper apt-installable .deb with bundled Qt6.
#
#  Usage: QT_PREFIX=/path/to/qt ./scripts/build-deb.sh [version]
#
#  Environment:
#    QT_PREFIX   — Qt6 install prefix (default: /opt/qt6/6.8.3/gcc_64)
#    DOTNET_ROOT — .NET SDK root (auto-detected if not set)
#    JOBS        — parallel build jobs (default: nproc)
# ============================================================
set -euo pipefail

VERSION="${1:-1.1.0}"
VERSION="${VERSION#v}"
VERSION=$(echo "$VERSION" | tr -c '[:alnum:].~+\n-' '.' | sed 's/\.$//')
VERSION="${VERSION:-1.1.0}"
ARCH="amd64"
PKG="oynix_${VERSION}_${ARCH}"
JOBS="${JOBS:-$(nproc)}"

echo "=== OyNIx .deb builder v${VERSION} (${ARCH}) ==="

# ── Prerequisites ──────────────────────────────────────────────
QT_PREFIX="${QT_PREFIX:-/opt/qt6/6.8.3/gcc_64}"
for cmd in cmake dpkg-deb; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: $cmd not found" >&2; exit 1
    fi
done

# Verify Qt prefix actually exists
if [ ! -d "${QT_PREFIX}/lib" ]; then
    echo "ERROR: Qt prefix not found at ${QT_PREFIX}" >&2
    echo "  Set QT_PREFIX to your Qt6 installation directory." >&2
    exit 1
fi

# Prefer Ninja if available
CMAKE_GENERATOR=""
if command -v ninja &>/dev/null; then
    CMAKE_GENERATOR="-GNinja"
fi

echo "  Qt:    ${QT_PREFIX}"
echo "  Jobs:  ${JOBS}"
echo "  Ninja: $(command -v ninja &>/dev/null && echo yes || echo no)"
echo ""

# ── Build with CMake ───────────────────────────────────────────
BUILD="_build_deb"
rm -rf "${BUILD}"
mkdir -p "${BUILD}"

cmake -S . -B "${BUILD}" \
    ${CMAKE_GENERATOR} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "${BUILD}" -j"${JOBS}"

# ── Assemble .deb tree ─────────────────────────────────────────
LIB_DIR="${PKG}/usr/lib/oynix"

rm -rf "${PKG}" "${PKG}.deb"
mkdir -p "${PKG}/DEBIAN"
mkdir -p "${PKG}/usr/bin"
mkdir -p "${LIB_DIR}"
mkdir -p "${PKG}/usr/share/applications"
mkdir -p "${PKG}/usr/share/oynix/themes"
mkdir -p "${PKG}/usr/share/doc/oynix"

# ── Binary ────────────────────────────────────────────────────
install -m755 "${BUILD}/oynix" "${LIB_DIR}/oynix"

# ── C# NativeAOT core library ─────────────────────────────────
if [ -f "${BUILD}/dotnet/OyNIxCore.so" ]; then
    install -m644 "${BUILD}/dotnet/OyNIxCore.so" "${LIB_DIR}/"
    echo "  Bundled OyNIxCore.so (NativeAOT)"
fi

# ── Bundle Qt6 libraries ──────────────────────────────────────
echo "Bundling Qt6 libraries..."
QT_LIB="${QT_PREFIX}/lib"

# 1) Direct dependencies from ldd
NEEDED_QT_LIBS=$(ldd "${LIB_DIR}/oynix" 2>/dev/null \
    | grep -oP "${QT_LIB}/\S+" | sort -u || true)
for lib in $NEEDED_QT_LIBS; do
    cp -L "$lib" "${LIB_DIR}/"
done

# 2) ICU libraries (required by QtCore)
for icu in "${QT_LIB}"/libicu*.so.*; do
    [ -f "$icu" ] || continue
    base=$(basename "$icu")
    [ -f "${LIB_DIR}/${base}" ] && continue
    cp -L "$icu" "${LIB_DIR}/"
done
# Deduplicate: keep shortest-named version of each ICU lib
for icu in "${LIB_DIR}"/libicu*.so.*.*; do
    [ -f "$icu" ] || continue
    short=$(echo "$icu" | sed 's/\.[0-9]*$//')
    [ -f "$short" ] && rm -f "$icu"
done

# 3) Qt plugins
for plugdir in platforms xcbglintegrations imageformats tls \
               egldeviceintegrations wayland-shell-integration; do
    src="${QT_PREFIX}/plugins/${plugdir}"
    if [ -d "$src" ]; then
        mkdir -p "${LIB_DIR}/plugins/${plugdir}"
        cp -L "$src"/*.so "${LIB_DIR}/plugins/${plugdir}/" 2>/dev/null || true
    fi
done

# 4) QtWebEngineProcess + resources
if [ -d "${QT_PREFIX}/libexec" ]; then
    mkdir -p "${LIB_DIR}/libexec"
    for f in QtWebEngineProcess; do
        [ -f "${QT_PREFIX}/libexec/$f" ] && \
            install -m755 "${QT_PREFIX}/libexec/$f" "${LIB_DIR}/libexec/"
    done
fi
if [ -d "${QT_PREFIX}/resources" ]; then
    mkdir -p "${LIB_DIR}/resources"
    cp -L "${QT_PREFIX}/resources"/* "${LIB_DIR}/resources/" 2>/dev/null || true
fi

# 5) Qt WebEngine translation packs
if [ -d "${QT_PREFIX}/translations" ]; then
    mkdir -p "${LIB_DIR}/translations"
    cp -L "${QT_PREFIX}/translations"/qtwebengine_locales/*.pak \
        "${LIB_DIR}/translations/" 2>/dev/null || true
fi

# 6) Resolve transitive dependencies (up to 3 rounds)
echo "Resolving transitive Qt dependencies..."
for round in 1 2 3; do
    MISSING=0
    for so in "${LIB_DIR}"/*.so* "${LIB_DIR}"/plugins/*/*.so* \
              "${LIB_DIR}"/libexec/*; do
        [ -f "$so" ] || continue
        DEPS=$(ldd "$so" 2>/dev/null | grep "not found" | awk '{print $1}' || true)
        for dep in $DEPS; do
            if [ -f "${QT_LIB}/${dep}" ] && [ ! -f "${LIB_DIR}/${dep}" ]; then
                cp -L "${QT_LIB}/${dep}" "${LIB_DIR}/"
                MISSING=1
            fi
        done
    done
    [ "$MISSING" = "0" ] && break
done

# Fix permissions
find "${LIB_DIR}" -name '*.so*' -exec chmod 644 {} +
find "${LIB_DIR}/libexec" -type f -exec chmod 755 {} + 2>/dev/null || true

LIB_COUNT=$(find "${LIB_DIR}" -name '*.so*' | wc -l)
echo "  Bundled ${LIB_COUNT} library files"

# ── Launcher wrapper ──────────────────────────────────────────
cat > "${PKG}/usr/bin/oynix" <<'LAUNCHER'
#!/bin/bash
# OyNIx Browser launcher — sets up bundled Qt environment
OYNIX_DIR="/usr/lib/oynix"
export LD_LIBRARY_PATH="${OYNIX_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
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
            dir="${PKG}/usr/share/icons/hicolor/${size}x${size}/apps"
            mkdir -p "$dir"
            install -m644 "$icon" "${dir}/oynix.png"
        fi
    done
    # Scalable SVG
    if [ -f "assets/icon.svg" ]; then
        mkdir -p "${PKG}/usr/share/icons/hicolor/scalable/apps"
        install -m644 "assets/icon.svg" "${PKG}/usr/share/icons/hicolor/scalable/apps/oynix.svg"
    fi
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
cat > "${PKG}/usr/share/doc/oynix/copyright" <<COPYRIGHT
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: OyNIx Browser
Source: https://github.com/minerofthesoal/OyNIx

Files: *
Copyright: 2026 OyNIx Project
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files, to deal in the Software
 without restriction.
COPYRIGHT

# ── postinst ──────────────────────────────────────────────────
cat > "${PKG}/DEBIAN/postinst" <<'POSTINST'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
fi
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database /usr/share/mime 2>/dev/null || true
fi
POSTINST
chmod 755 "${PKG}/DEBIAN/postinst"

# ── postrm ────────────────────────────────────────────────────
cat > "${PKG}/DEBIAN/postrm" <<'POSTRM'
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
    fi
fi
if [ "$1" = "purge" ]; then
    rm -rf /usr/share/oynix 2>/dev/null || true
    rm -rf /usr/lib/oynix 2>/dev/null || true
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
Depends: libc6 (>= 2.35), libegl1, libgl1, libglib2.0-0,
 libfontconfig1, libfreetype6, libxkbcommon0, libnss3, libnspr4,
 libxcomposite1, libxdamage1, libxrandr2, libxtst6,
 libx11-6, libx11-xcb1, libxcb1, libxext6, libxfixes3,
 libdbus-1-3, libasound2 | libasound2t64, libpulse0,
 libdrm2, libgbm1, libexpat1
CTRL

# ── Build .deb ────────────────────────────────────────────────
dpkg-deb --root-owner-group --build "${PKG}"

OUTPUT="oynix-${VERSION}-${ARCH}.deb"
mv "${PKG}.deb" "${OUTPUT}"

# ── Verify ────────────────────────────────────────────────────
echo ""
echo "=== Package info ==="
dpkg-deb --info "${OUTPUT}" 2>/dev/null | grep -E '^\s*(Package|Version|Architecture|Installed-Size|Depends|Section)' || true
echo ""
echo "=== Top-level contents ==="
dpkg-deb --contents "${OUTPUT}" | head -25
echo "  ..."

DEB_SIZE=$(du -sh "${OUTPUT}" | awk '{print $1}')
echo ""
echo "=== Done ==="
echo "  File: ${OUTPUT} (${DEB_SIZE})"
echo "  Install: sudo apt install ./${OUTPUT}"

rm -rf "${PKG}" "${BUILD}"
