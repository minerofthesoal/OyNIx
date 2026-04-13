#!/bin/bash
# ============================================================
#  OyNIx Browser — Build apt package via dpkg-buildpackage
#
#  Produces a proper Debian/Ubuntu/Mint .deb using the debian/
#  directory (debhelper + cmake).  When QT_PREFIX is set, Qt6
#  libraries are bundled for portability.
#
#  Usage:
#    ./scripts/build-deb.sh [version]
#
#  Environment:
#    QT_PREFIX   — Qt6 install prefix (bundles Qt if set)
#    QT_ROOT_DIR — alternative (set by install-qt-action)
#    Qt6_DIR     — alternative (cmake-style)
#    JOBS        — parallel build jobs (default: nproc)
# ============================================================
set -euo pipefail

VERSION="${1:-3.1.0}"
VERSION="${VERSION#v}"
VERSION=$(echo "$VERSION" | tr -c '[:alnum:].~+\n-' '.' | sed 's/\.$//')
VERSION="${VERSION:-3.1.0}"
JOBS="${JOBS:-$(nproc)}"

echo "=== OyNIx apt package builder v${VERSION} ==="

# ── Prerequisites ────────────────────────────────────────────
for cmd in cmake dpkg-buildpackage debhelper; do
    case "$cmd" in
        debhelper)
            # debhelper is a package, not a command — check dh
            if ! command -v dh &>/dev/null; then
                echo "ERROR: debhelper not found — install with:" >&2
                echo "  sudo apt install debhelper devscripts" >&2
                exit 1
            fi
            ;;
        *)
            if ! command -v "$cmd" &>/dev/null; then
                echo "ERROR: $cmd not found" >&2
                exit 1
            fi
            ;;
    esac
done

# ── Resolve Qt prefix ───────────────────────────────────────
if [ -z "${QT_PREFIX:-}" ]; then
    if [ -n "${QT_ROOT_DIR:-}" ] && [ -d "${QT_ROOT_DIR}/lib" ]; then
        export QT_PREFIX="${QT_ROOT_DIR}"
    elif [ -n "${Qt6_DIR:-}" ]; then
        _candidate="$(cd "${Qt6_DIR}/../../.." 2>/dev/null && pwd)"
        if [ -d "${_candidate}/lib" ]; then
            export QT_PREFIX="${_candidate}"
        fi
    fi
fi

if [ -n "${QT_PREFIX:-}" ]; then
    echo "  Qt prefix: ${QT_PREFIX} (will bundle)"
    export QT_PREFIX
else
    echo "  Qt prefix: system (no bundling)"
fi
echo "  Jobs:      ${JOBS}"
echo ""

# ── Update debian/changelog with requested version ──────────
TIMESTAMP=$(date -R)
cat > debian/changelog <<CHLOG
oynix (${VERSION}) unstable; urgency=medium

  * Package build ${VERSION}

 -- OyNIx Project <oynix@users.noreply.github.com>  ${TIMESTAMP}
CHLOG

# ── Build with dpkg-buildpackage ─────────────────────────────
echo "Building apt package with dpkg-buildpackage..."
echo ""

export DEB_BUILD_OPTIONS="parallel=${JOBS}"

dpkg-buildpackage -b -us -uc -j"${JOBS}"

# ── Collect output ───────────────────────────────────────────
# dpkg-buildpackage places .deb in parent directory
DEB=$(ls ../oynix_${VERSION}_*.deb 2>/dev/null | head -1)

if [ -z "$DEB" ]; then
    echo "ERROR: No .deb produced" >&2
    exit 1
fi

# Move to project root for convenience
OUTPUT="oynix-${VERSION}-amd64.deb"
mv "$DEB" "./${OUTPUT}"

# Clean up other dpkg-buildpackage artifacts from parent dir
rm -f ../oynix_${VERSION}_*.buildinfo ../oynix_${VERSION}_*.changes 2>/dev/null || true

# ── Verify ───────────────────────────────────────────────────
echo ""
echo "=== Package info ==="
dpkg-deb --info "${OUTPUT}" 2>/dev/null \
    | grep -E '^\s*(Package|Version|Architecture|Installed-Size|Depends|Section)' || true
echo ""
echo "=== Top-level contents ==="
dpkg-deb --contents "${OUTPUT}" 2>/dev/null | head -25 || true
echo "  ..."

DEB_SIZE=$(du -sh "${OUTPUT}" | awk '{print $1}')
echo ""
echo "=== Done ==="
echo "  File: ${OUTPUT} (${DEB_SIZE})"
echo "  Install:   sudo apt install ./${OUTPUT}"
echo "  Verify:    dpkg-deb --info ${OUTPUT}"
echo "  Contents:  dpkg-deb --contents ${OUTPUT}"
