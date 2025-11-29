#!/bin/bash
# Build script for NAGI (SDL3)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="${1:-Release}"

echo "=== NAGI Build Script ==="
echo "Build type: $BUILD_TYPE"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# Build
echo ""
echo "Building..."
cmake --build . --parallel

echo ""
echo "=== Build complete ==="
echo "Executable: $BUILD_DIR/nagi"
