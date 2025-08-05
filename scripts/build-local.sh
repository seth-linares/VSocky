#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "Building vsocky locally..."
echo "Build type: $BUILD_TYPE"

# Create directory structure first
if [ ! -d "include/vsocky" ]; then
    echo "Setting up directory structure..."
    ./scripts/setup-dirs.sh
fi

# Configure
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_STATIC=OFF \
    -DUSE_SIMDJSON=ON \
    -DBUILD_TESTS=ON

# Build
cmake --build "$BUILD_DIR" -j$(nproc)

# Run tests if they exist
if [ -f "$BUILD_DIR/tests/test_utils" ]; then
    echo ""
    echo "Running unit tests..."
    echo "===================="
    cd "$BUILD_DIR" && ctest --output-on-failure
    cd - > /dev/null
fi

# Test the main binary
echo ""
echo "Testing vsocky binary..."
echo "======================="
"$BUILD_DIR/vsocky" --version
echo ""
"$BUILD_DIR/vsocky" --help

# Show binary info
echo ""
echo "Binary information:"
echo "==================="
file "$BUILD_DIR/vsocky"
echo -n "Size: "
ls -lh "$BUILD_DIR/vsocky" | awk '{print $5}'
echo -n "Dynamic libraries: "
ldd "$BUILD_DIR/vsocky" 2>/dev/null | wc -l || echo "0 (static)"

echo ""
echo "Build complete! Binary at: $BUILD_DIR/vsocky"