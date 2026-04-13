#!/bin/bash
# ============================================================
#  OyNIx Browser v3.1 — Quick Build Script
#
#  Usage:
#    ./build.sh              # Release build
#    ./build.sh debug        # Debug build
#    ./build.sh clean        # Remove build artifacts
#    ./build.sh run          # Build + run
#    ./build.sh install      # Build + install to ~/.local
#
#  For full Ubuntu/Mint setup with dependency management:
#    ./scripts/build-ubuntu.sh --help
# ============================================================
set -e

MODE="${1:-release}"

# ── Colors ───────────────────────────────────────────────────
PURPLE='\033[0;35m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${PURPLE}${BOLD}"
echo "  OyNIx Browser v3.1 — Build"
echo "  =========================="
echo -e "${NC}"

# ── Handle clean ─────────────────────────────────────────────
if [ "$MODE" = "clean" ]; then
    echo -e "${CYAN}Cleaning build artifacts...${NC}"
    rm -rf build _build_ubuntu _build_deb
    echo -e "${GREEN}Clean complete${NC}"
    exit 0
fi

# ── Determine build type ─────────────────────────────────────
BUILD_TYPE="Release"
if [ "$MODE" = "debug" ]; then
    BUILD_TYPE="Debug"
fi

BUILD_DIR="build"
NPROC=$(nproc 2>/dev/null || echo 4)

echo -e "  Mode:    ${CYAN}${BUILD_TYPE}${NC}"
echo -e "  Threads: ${CYAN}${NPROC}${NC}"

# ── Check dependencies ───────────────────────────────────────
echo ""
echo -e "${CYAN}Checking dependencies...${NC}"

MISSING=0
for cmd in cmake g++; do
    if ! command -v "$cmd" &>/dev/null; then
        echo -e "  ${RED}Missing: $cmd${NC}"
        MISSING=1
    else
        echo -e "  ${GREEN}Found: $cmd${NC}"
    fi
done

# Check Qt6
if pkg-config --exists Qt6Core 2>/dev/null; then
    QT_VER=$(pkg-config --modversion Qt6Core 2>/dev/null || echo "found")
    echo -e "  ${GREEN}Found: Qt6 (${QT_VER})${NC}"
elif [ -n "${QT_ROOT_DIR:-}" ]; then
    echo -e "  ${GREEN}Found: Qt6 (${QT_ROOT_DIR})${NC}"
else
    # Check common Ubuntu/Mint package names
    if dpkg -l qt6-base-dev &>/dev/null 2>&1; then
        echo -e "  ${GREEN}Found: Qt6 (system packages)${NC}"
    else
        echo -e "  ${YELLOW}Qt6 not detected — cmake will try to find it${NC}"
    fi
fi

# Check .NET
if command -v dotnet &>/dev/null; then
    DOTNET_VER=$(dotnet --version 2>/dev/null || echo "found")
    echo -e "  ${GREEN}Found: .NET SDK ${DOTNET_VER}${NC}"
else
    echo -e "  ${YELLOW}.NET SDK not found — C# core will be skipped${NC}"
fi

if [ "$MISSING" = "1" ]; then
    echo ""
    echo -e "${YELLOW}Some tools are missing. Install with:${NC}"
    echo "  ./scripts/build-ubuntu.sh --deps-only"
    echo ""
    read -rp "Continue anyway? [y/N] " ans
    if [[ ! "$ans" =~ ^[Yy] ]]; then
        exit 1
    fi
fi

# ── Resolve Qt prefix ────────────────────────────────────────
CMAKE_PREFIX=""
if [ -n "${QT_PREFIX:-}" ] && [ -d "${QT_PREFIX}/lib" ]; then
    CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=${QT_PREFIX}"
elif [ -n "${QT_ROOT_DIR:-}" ] && [ -d "${QT_ROOT_DIR}/lib" ]; then
    CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=${QT_ROOT_DIR}"
elif [ -n "${Qt6_DIR:-}" ]; then
    candidate="$(cd "${Qt6_DIR}/../../.." 2>/dev/null && pwd)"
    if [ -d "${candidate}/lib" ]; then
        CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=${candidate}"
    fi
fi

# ── Prefer Ninja ─────────────────────────────────────────────
CMAKE_GEN=""
if command -v ninja &>/dev/null; then
    CMAKE_GEN="-GNinja"
    echo -e "  ${GREEN}Using Ninja${NC}"
fi

# ── Configure ────────────────────────────────────────────────
echo ""
echo -e "${CYAN}Configuring with CMake...${NC}"
mkdir -p "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" \
    ${CMAKE_GEN} \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    ${CMAKE_PREFIX}

# ── Build ────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}Building with ${NPROC} threads...${NC}"
cmake --build "${BUILD_DIR}" -j"${NPROC}"

# ── Result ───────────────────────────────────────────────────
if [ ! -f "${BUILD_DIR}/oynix" ]; then
    echo -e "${RED}Build failed — binary not found${NC}"
    exit 1
fi

BINARY_SIZE=$(du -sh "${BUILD_DIR}/oynix" | awk '{print $1}')
echo ""
echo -e "${GREEN}${BOLD}Build successful!${NC}"
echo ""
echo "  Binary: ${BUILD_DIR}/oynix (${BINARY_SIZE})"

if [ -f "${BUILD_DIR}/dotnet/OyNIxCore.so" ]; then
    CORE_SIZE=$(du -sh "${BUILD_DIR}/dotnet/OyNIxCore.so" | awk '{print $1}')
    echo "  C# Core: ${BUILD_DIR}/dotnet/OyNIxCore.so (${CORE_SIZE})"
fi

# ── Post-build actions ───────────────────────────────────────
if [ "$MODE" = "run" ]; then
    echo ""
    echo -e "${CYAN}Launching OyNIx...${NC}"
    exec "${BUILD_DIR}/oynix"
fi

if [ "$MODE" = "install" ]; then
    echo ""
    echo -e "${CYAN}Installing to ~/.local...${NC}"
    exec ./scripts/build-ubuntu.sh --install
fi

echo ""
echo "  Run:     ./${BUILD_DIR}/oynix"
echo "  Install: ./scripts/build-ubuntu.sh --install"
echo "  .deb:    ./scripts/build-ubuntu.sh --deb"
echo ""
