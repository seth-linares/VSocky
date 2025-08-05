#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build-alpine"

echo "Building vsocky for Alpine (static binary)..."
echo "Build type: $BUILD_TYPE"

# Run the build in Alpine container
docker run --rm \
    -v "$PWD:/workspace" \
    -w /workspace \
    -u "$(id -u):$(id -g)" \
    vsocky-alpine-build:latest \
    sh -c "
        # Create directory structure
        if [ ! -d 'include/vsocky' ]; then
            echo 'Setting up directory structure...'
            sh scripts/setup-dirs.sh
        fi
        
        # Configure
        cmake -B '$BUILD_DIR' \
            -DCMAKE_BUILD_TYPE='$BUILD_TYPE' \
            -DBUILD_STATIC=ON \
            -DUSE_SIMDJSON=ON \
            -DBUILD_TESTS=ON
        
        # Build
        cmake --build '$BUILD_DIR' -j\$(nproc)
        
        # Run tests
        if [ -f '$BUILD_DIR/tests/test_utils' ]; then
            echo ''
            echo 'Running unit tests...'
            echo '===================='
            cd '$BUILD_DIR' && ctest --output-on-failure
            cd - > /dev/null
        fi
    "

# Test the binary outside container
echo ""
echo "Testing vsocky binary..."
echo "======================="
"$BUILD_DIR/vsocky" --version || echo "Note: Binary may not run on host if not Alpine"

# Show binary info
echo ""
echo "Binary information:"
echo "==================="
file "$BUILD_DIR/vsocky"
echo -n "Size: "
ls -lh "$BUILD_DIR/vsocky" | awk '{print $5}'

# Check if truly static
echo ""
echo "Static analysis:"
if ldd "$BUILD_DIR/vsocky" 2>&1 | grep -q "not a dynamic executable"; then
    echo "✓ Binary is statically linked"
else
    echo "⚠ Binary appears to have dynamic dependencies:"
    ldd "$BUILD_DIR/vsocky" 2>/dev/null || true
fi

# Symbols check
echo ""
echo "Binary size breakdown:"
size "$BUILD_DIR/vsocky" 2>/dev/null || true

echo ""
echo "Build complete! Static binary at: $BUILD_DIR/vsocky"