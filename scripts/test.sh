#!/bin/bash
set -e

# Run tests in both environments
echo "Running tests..."

# Local tests (if available)
if [ -d "build-local" ] && [ -f "build-local/vsocky_test" ]; then
    echo "Running local tests..."
    ./build-local/vsocky_test
fi

# Alpine tests
echo "Running Alpine tests..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    vsocky-alpine-build \
    sh -c "cd build-alpine && ctest --verbose"

echo "All tests passed!"