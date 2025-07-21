#!/bin/bash
# scripts/dev.sh - Quick development iteration script
#
# Usage: ./scripts/dev.sh [ACTION] [TARGET]
#
# Actions:
#   build    - Build vsocky for the specified target (default)
#   test     - Run the built binary with --test-json
#   clean    - Remove all build directories
#   shell    - Start an interactive development shell
#   size     - Show binary sizes for all built targets
#   analyze  - Analyze what's making the binary large
#
# Targets:
#   local   - Ubuntu/WSL2 development build (default)
#   alpine  - Alpine Linux static build
#
# Examples:
#   ./scripts/dev.sh                    # Build for local (default)
#   ./scripts/dev.sh build alpine       # Build Alpine static binary
#   ./scripts/dev.sh test local         # Test local build
#   ./scripts/dev.sh shell alpine       # Start Alpine dev shell
#   ./scripts/dev.sh clean              # Clean all builds
#   ./scripts/dev.sh size               # Show all binary sizes
#   ./scripts/dev.sh analyze            # Analyze binary composition

set -e

ACTION="${1:-build}"
TARGET="${2:-local}"

case "$ACTION" in
    build)
        echo "Building for $TARGET..."
        if [ "$TARGET" == "alpine" ]; then
            ./scripts/build-alpine.sh Release
        else
            ./scripts/build-local.sh Release
        fi
        ;;
    
    test)
        echo "Running tests..."
        if [ -f "build-$TARGET/vsocky" ]; then
            "./build-$TARGET/vsocky" --test-json
        else
            echo "Error: No binary found for $TARGET"
            exit 1
        fi
        ;;
    
    clean)
        echo "Cleaning build directories..."
        rm -rf build-local build-alpine build-alpine-no-json build-alpine-debug
        echo "Clean complete!"
        ;;
    
    shell)
        echo "Starting development shell for $TARGET..."
        ./scripts/dev-shell.sh "$TARGET"
        ;;
    
    size)
        echo "Binary sizes:"
        echo "============="
        for binary in build-*/vsocky; do
            if [ -f "$binary" ]; then
                printf "%-30s %s\n" "$binary:" "$(ls -lh "$binary" | awk '{print $5}')"
            fi
        done
        ;;
    
    analyze)
        echo "Analyzing binary composition..."
        ./scripts/analyze-binary.sh
        ;;
    
    *)
        echo "Usage: $0 [build|test|clean|shell|size|analyze|compress] [local|alpine]"
        echo "Run with no arguments for default (build local)"
        echo "See script header for detailed examples"
        exit 1
        ;;
esac