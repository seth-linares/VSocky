#!/bin/bash
# scripts/analyze-binary.sh - Analyze what's making the binary large

set -e

echo "VSocky Binary Size Analysis"
echo "==========================="
echo ""

# Build without simdjson to see size difference
echo "1. Building WITHOUT simdjson for comparison..."
docker run --rm -it \
    -v "$(pwd):/workspace" \
    -w /workspace \
    vsocky-alpine-build \
    sh -c "
        cmake -B build-alpine-no-json \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_STATIC=ON \
            -DUSE_SIMDJSON=OFF \
            -DCMAKE_VERBOSE_MAKEFILE=OFF
        cmake --build build-alpine-no-json -j\$(nproc)
    "

echo ""
echo "2. Size Comparison:"
echo "-------------------"
if [ -f "build-alpine/vsocky" ]; then
    echo -n "WITH simdjson:    "
    ls -lh build-alpine/vsocky | awk '{print $5}'
fi
if [ -f "build-alpine-no-json/vsocky" ]; then
    echo -n "WITHOUT simdjson: "
    ls -lh build-alpine-no-json/vsocky | awk '{print $5}'
fi

echo ""
echo "3. Symbol Analysis (top 20 largest symbols):"
echo "--------------------------------------------"
if [ -f "build-alpine/vsocky" ]; then
    # Need to build with symbols to analyze
    echo "Building debug version for symbol analysis..."
    docker run --rm -it \
        -v "$(pwd):/workspace" \
        -w /workspace \
        vsocky-alpine-build \
        sh -c "
            cmake -B build-alpine-debug \
                -DCMAKE_BUILD_TYPE=RelWithDebInfo \
                -DBUILD_STATIC=ON \
                -DUSE_SIMDJSON=ON
            cmake --build build-alpine-debug -j\$(nproc)
            
            echo 'Top 20 largest symbols:'
            nm --print-size --size-sort --radix=d build-alpine-debug/vsocky | tail -20
        "
fi

echo ""
echo "4. Section sizes:"
echo "-----------------"
if [ -f "build-alpine/vsocky" ]; then
    size -A build-alpine/vsocky
fi

echo ""
echo "5. Recommendations:"
echo "-------------------"
echo "- Current simdjson adds significant size (~140KB)"
echo "- Consider using a lighter JSON parser for smaller binary"
echo "- Or implement minimal custom JSON parsing"
echo "- UPX compression can reduce size by 50-70%"