#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build-local"

echo "Building vsocky locally..."
echo "Build type: $BUILD_TYPE"

# Configure
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_STATIC=OFF \
    -DUSE_SIMDJSON=ON

# Build
cmake --build "$BUILD_DIR" -j$(nproc)

# Run tests
echo ""
echo "Testing binary..."
echo "================="
"$BUILD_DIR/vsocky" --version
echo ""
"$BUILD_DIR/vsocky" --test-json

# Show binary info
echo ""
echo "Binary information:"
echo "==================="
file "$BUILD_DIR/vsocky"
echo -n "Size: "
ls -lh "$BUILD_DIR/vsocky" | awk '{print $5}'
echo -n "Dynamic libraries: "
ldd "$BUILD_DIR/vsocky" | wc -l

echo ""
echo "Build complete! Binary at: $BUILD_DIR/vsocky"