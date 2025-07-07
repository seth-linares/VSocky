#!/bin/bash
# scripts/build_static.sh

set -e  # Exit on error

BUILD_DIR="build-static"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTING=OFF \
      -DENABLE_DEBUG_LOGGING=OFF \
      ..

make -j$(nproc)

# Check binary size
echo "Binary size:"
ls -lh vsocky
file vsocky

# Verify static linking
echo -e "\nDynamic dependencies (should be minimal):"
ldd vsocky || echo "No dynamic dependencies (good!)"