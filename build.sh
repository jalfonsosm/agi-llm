#!/bin/bash
# Build script for NAGI (SDL3)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="${1:-Release}"
MODEL_DIR="$BUILD_DIR/models"

echo "=== NAGI Build Script ==="
echo "Build type: $BUILD_TYPE"

mkdir -p "$BUILD_DIR"
mkdir -p "$MODEL_DIR"
cd "$BUILD_DIR"

# The model download is now handled by CMake during configure. This
# script only configures and builds the project; if you want to control
# model provisioning manually, set the env var HF_TOKEN and ensure the
# model exists under build/models/, or pass -DNAGI_DOWNLOAD_MODEL=OFF to
# disable automatic download in CMake.
MODEL_PATH=""

echo "Configuring..."
# We always enable LLM; static linking is handled by CMake which now attempts
# a static build of llama.cpp by default.
CM_FLAGS=( -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DNAGI_ENABLE_LLM=ON )

echo "cmake .. ${CM_FLAGS[*]}"
cmake .. "${CM_FLAGS[@]}"

echo "Building..."
cmake --build . --parallel

echo "=== Build complete ==="
echo "Executable: $BUILD_DIR/nagi"
