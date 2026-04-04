#!/bin/bash
# OyNIx Browser v3.0 - Build Script
# Usage: ./build.sh [debug|release]

set -e

MODE="${1:-release}"
BUILD_DIR="build/${MODE}"

echo ""
echo "  OyNIx Browser v3.0 - C++ Build"
echo "  ================================"
echo "  Mode: ${MODE}"
echo ""

# Check dependencies
echo "  Checking dependencies..."
for pkg in cmake g++ qt6-base-dev qt6-webengine-dev libsqlite3-dev; do
    if ! dpkg -l "$pkg" &>/dev/null && ! pacman -Q "$pkg" &>/dev/null 2>&1; then
        echo "  WARNING: $pkg may not be installed"
    fi
done

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "  Configuring with CMake..."
if [ "$MODE" = "debug" ]; then
    cmake ../.. -DCMAKE_BUILD_TYPE=Debug
else
    cmake ../.. -DCMAKE_BUILD_TYPE=Release
fi

# Build
NPROC=$(nproc 2>/dev/null || echo 4)
echo "  Building with ${NPROC} threads..."
cmake --build . -j"$NPROC"

echo ""
echo "  Build complete: ${BUILD_DIR}/oynix"
echo "  Run: ./${BUILD_DIR}/oynix"
echo ""
