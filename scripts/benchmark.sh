#!/bin/bash
set -e

# Run performance benchmarks
echo "Running vsocky benchmarks..."

# Build optimized version if not exists
if [ ! -f "build-local/vsocky" ]; then
    ./scripts/build-local.sh Release
fi

# Simple startup time benchmark
echo "Measuring startup time..."
time for i in {1..100}; do
    ./build-local/vsocky --version > /dev/null
done

# JSON parsing benchmark is built-in
echo ""
echo "JSON parsing benchmark:"
./build-local/vsocky --test-json

# Memory usage
echo ""
echo "Memory usage:"
/usr/bin/time -v ./build-local/vsocky --version 2>&1 | grep -E "(Maximum resident|User time|System time)"