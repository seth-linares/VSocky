#!/bin/bash
# scripts/check-env.sh - Verify development environment is ready

set -e

echo "VSocky Development Environment Check"
echo "===================================="
echo ""

# Check host environment
echo "Host Environment:"
echo "  OS: $(uname -s) $(uname -r)"
echo "  Architecture: $(uname -m)"
echo "  CMake: $(cmake --version | head -n1)"
echo "  GCC: $(gcc --version | head -n1)"
echo "  Docker: $(docker --version)"
echo ""

# Check Docker images
echo "Docker Images:"
for image in vsocky-alpine-build vsocky-ubuntu-build; do
    if docker images | grep -q "$image"; then
        echo "  ✓ $image"
    else
        echo "  ✗ $image (missing - run ./scripts/setup.sh)"
    fi
done
echo ""

# Check binaries
echo "Built Binaries:"
for binary in build/vsocky build-alpine/vsocky; do
    if [ -f "$binary" ]; then
        SIZE=$(ls -lh "$binary" | awk '{print $5}')
        echo "  ✓ $binary ($SIZE)"
        # Check if it's actually static (Alpine)
        if [[ "$binary" == *"alpine"* ]]; then
            if ldd "$binary" 2>&1 | grep -q "not a dynamic executable"; then
                echo "    → Static binary ✓"
            else
                echo "    → Warning: Not fully static!"
            fi
        fi
    else
        echo "  ✗ $binary (not built yet)"
    fi
done
echo ""

# Check simdjson
echo "Dependencies:"
if [ -d "build/_deps/simdjson-src" ]; then
    echo "  ✓ simdjson (fetched)"
else
    echo "  ✗ simdjson (will be fetched on first build)"
fi
echo ""

# Performance check
if [ -f "build/vsocky" ]; then
    echo "Quick Performance Test:"
    TIME=$( { time ./build/vsocky --version >/dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}')
    echo "  Startup time: $TIME"
fi

echo ""
echo "Environment check complete!"