#!/bin/bash
# Development helper script

set -e

COMMAND="${1:-help}"

case "$COMMAND" in
    test)
        TARGET="${2:-local}"
        echo "Running tests for $TARGET build..."
        
        if [ "$TARGET" = "local" ]; then
            BUILD_DIR="build"
        else
            BUILD_DIR="build-alpine"
        fi
        
        if [ -d "$BUILD_DIR" ] && [ -f "$BUILD_DIR/tests/test_utils" ]; then
            cd "$BUILD_DIR" && ctest --output-on-failure
        else
            echo "No tests found. Build first with: make build"
        fi
        ;;
        
    run)
        TARGET="${2:-local}"
        shift 2
        
        if [ "$TARGET" = "local" ]; then
            BUILD_DIR="build"
        else
            BUILD_DIR="build-alpine"
        fi
        
        if [ -f "$BUILD_DIR/vsocky" ]; then
            "$BUILD_DIR/vsocky" "$@"
        else
            echo "Binary not found. Build first with: make build"
        fi
        ;;
        
    clean)
        echo "Cleaning build directories..."
        rm -rf build build-alpine build-*
        echo "Clean complete!"
        ;;
        
    size)
        echo "Binary sizes:"
        echo "============="
        
        if [ -f "build/vsocky" ]; then
            echo -n "Local build:  "
            ls -lh build/vsocky | awk '{print $5}'
            size build/vsocky
        fi
        
        echo ""
        
        if [ -f "build-alpine/vsocky" ]; then
            echo -n "Alpine build: "
            ls -lh build-alpine/vsocky | awk '{print $5}'
            size build-alpine/vsocky 2>/dev/null || echo "(size command may not work for Alpine binary on non-Alpine host)"
        fi
        ;;
        
    deps)
        echo "Checking dependencies..."
        
        if [ -f "build/vsocky" ]; then
            echo "Local build dependencies:"
            ldd build/vsocky || echo "No dynamic dependencies"
        fi
        
        echo ""
        
        if [ -f "build-alpine/vsocky" ]; then
            echo "Alpine build dependencies:"
            ldd build-alpine/vsocky 2>&1 | grep -q "not a dynamic executable" && \
                echo "✓ Static binary (no dependencies)" || \
                ldd build-alpine/vsocky 2>/dev/null || echo "Cannot check on this system"
        fi
        ;;
        
    help|*)
        echo "VSocky development helper"
        echo ""
        echo "Usage: ./scripts/dev.sh [command] [options]"
        echo ""
        echo "Commands:"
        echo "  test [local|alpine]    Run tests for specified build"
        echo "  run [local|alpine] ... Run vsocky with arguments"
        echo "  clean                  Remove all build directories"
        echo "  size                   Show binary sizes"
        echo "  deps                   Check binary dependencies"
        echo "  help                   Show this help"
        echo ""
        echo "Examples:"
        echo "  ./scripts/dev.sh test local"
        echo "  ./scripts/dev.sh run local --version"
        echo "  ./scripts/dev.sh run alpine --help"
        ;;
esac