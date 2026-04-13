#!/bin/bash
# ============================================================
#  bundle-qt.sh — Bundle Qt6 libraries into the package
#
#  Called from debian/rules when QT_PREFIX is set (non-system Qt).
#  Usage: debian/bundle-qt.sh <lib_dir> <qt_prefix>
# ============================================================
set -euo pipefail

LIB_DIR="$1"
QT_PREFIX="$2"
QT_LIB="${QT_PREFIX}/lib"

if [ ! -d "${QT_LIB}" ]; then
    echo "ERROR: Qt lib dir not found: ${QT_LIB}" >&2
    exit 1
fi

# ── 1) Direct dependencies from ldd ─────────────────────────
NEEDED_QT_LIBS=$(ldd "${LIB_DIR}/oynix" 2>/dev/null \
    | grep -oP "${QT_LIB}/\S+" | sort -u || true)
for lib in $NEEDED_QT_LIBS; do
    cp -L "$lib" "${LIB_DIR}/"
done

# ── 2) ICU libraries (required by QtCore) ───────────────────
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

# ── 3) Qt plugins ───────────────────────────────────────────
for plugdir in platforms xcbglintegrations imageformats tls \
               egldeviceintegrations wayland-shell-integration; do
    src="${QT_PREFIX}/plugins/${plugdir}"
    if [ -d "$src" ]; then
        mkdir -p "${LIB_DIR}/plugins/${plugdir}"
        cp -L "$src"/*.so "${LIB_DIR}/plugins/${plugdir}/" 2>/dev/null || true
    fi
done

# ── 4) QtWebEngineProcess + resources ────────────────────────
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

# ── 5) Qt WebEngine translation packs ───────────────────────
if [ -d "${QT_PREFIX}/translations" ]; then
    mkdir -p "${LIB_DIR}/translations"
    cp -L "${QT_PREFIX}/translations"/qtwebengine_locales/*.pak \
        "${LIB_DIR}/translations/" 2>/dev/null || true
fi

# ── 6) Resolve transitive dependencies (up to 3 rounds) ─────
echo "  Resolving transitive Qt dependencies..."
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

# ── Fix permissions ──────────────────────────────────────────
find "${LIB_DIR}" -name '*.so*' -exec chmod 644 {} +
find "${LIB_DIR}/libexec" -type f -exec chmod 755 {} + 2>/dev/null || true

LIB_COUNT=$(find "${LIB_DIR}" -name '*.so*' | wc -l)
echo "  Bundled ${LIB_COUNT} Qt6 library files"
