#!/bin/bash
# Build project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "==================================="
echo "  Building Cap'n Proto RPC Demo"
echo "==================================="

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Run CMake
echo "Running CMake configuration..."
cmake ..

# Compile
echo "Compiling project..."
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete!"
echo ""
echo "Executables located at:"
echo "  ${BUILD_DIR}/core"
echo "  ${BUILD_DIR}/ext"
echo ""
echo "Run examples:"
echo "  1. Start Core:  ${BUILD_DIR}/core"
echo "  2. Start Ext1: ${BUILD_DIR}/ext ext1 9001 127.0.0.1:9000"
echo "  3. Start Ext2: ${BUILD_DIR}/ext ext2 9002 127.0.0.1:9000"
