#!/bin/bash
# Build script for test_nvidia_apis

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_DIR"
mkdir -p build_test
cd build_test

# Use the test CMakeLists
cp "$PROJECT_DIR/CMakeLists_test.txt" CMakeLists.txt

echo "Configuring CMake..."
cmake .

echo "Building..."
make -j$(nproc)

if [ -f test_nvidia_apis ]; then
    echo ""
    echo "Build successful!"
    echo "Run with: $PROJECT_DIR/build_test/test_nvidia_apis"
    echo "Or: cd build_test && ./test_nvidia_apis"
else
    echo "Build failed - executable not found!"
    exit 1
fi

