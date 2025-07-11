#!/bin/bash
set -e

BUILD_TYPE="${1:-Release}"
CLEAN_BUILD="${2:-}"

echo "Building vsocky for Alpine Linux..."
echo "Build type: $BUILD_TYPE"

# Clean build directory if requested
if [ "$CLEAN_BUILD" == "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf build-alpine
fi

# Create build directory on host to preserve outputs
mkdir -p build-alpine

# Run build in Alpine container
docker run --rm -it \
    -v "$(pwd):/workspace" \
    -w /workspace \
    -e TERM=xterm-256color \
    vsocky-alpine-build \
    sh -c "
        set -e
        echo 'Configuring build...'
        cmake -B build-alpine \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            -DBUILD_STATIC=ON \
            -DUSE_SIMDJSON=ON \
            -DCMAKE_VERBOSE_MAKEFILE=OFF
        
        echo ''
        echo 'Building vsocky...'
        cmake --build build-alpine -j\$(nproc)
        
        echo ''
        echo 'Build complete! Testing binary...'
        echo '================================'
        ./build-alpine/vsocky --version
        echo ''
        ./build-alpine/vsocky --test-json
        echo ''
        echo 'Binary information:'
        echo '=================='
        file ./build-alpine/vsocky
        echo ''
        echo -n 'Size: '
        du -h ./build-alpine/vsocky | cut -f1
        echo ''
        echo 'Dependencies:'
        ldd ./build-alpine/vsocky 2>/dev/null || echo '  No dynamic dependencies (static binary)'
        echo ''
        echo -n 'Dynamic symbols: '
        nm -D ./build-alpine/vsocky 2>/dev/null | wc -l
        echo ''
        # Show detailed size breakdown
        echo 'Size breakdown:'
        size ./build-alpine/vsocky 2>/dev/null || true
    "

echo ""
echo "Alpine build complete!"
echo "Binary location: ./build-alpine/vsocky"
echo ""
echo "To test in Alpine environment:"
echo "  docker run --rm -it -v \$(pwd):/workspace vsocky-alpine-build"
echo "  /workspace/build-alpine/vsocky --help"