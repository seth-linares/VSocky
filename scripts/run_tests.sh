#!/bin/bash
# scripts/run_tests.sh
# Comprehensive testing script for vsocky project

set -e  # Exit on first error - we want to know immediately if something fails

# Color codes for pretty output - makes it easier to spot problems
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
BUILD_DIR="build-test"
COVERAGE_DIR="coverage"
ENABLE_COVERAGE=${ENABLE_COVERAGE:-0}  # Set ENABLE_COVERAGE=1 for coverage reports
ENABLE_VALGRIND=${ENABLE_VALGRIND:-0}  # Set ENABLE_VALGRIND=1 for memory checks
VERBOSE=${VERBOSE:-0}  # Set VERBOSE=1 for detailed output

# Helper function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Helper function to run a command and check its result
run_test() {
    local test_name=$1
    local command=$2
    
    print_status "$BLUE" "Running: $test_name"
    
    if [ "$VERBOSE" -eq 1 ]; then
        # In verbose mode, show all output
        if eval "$command"; then
            print_status "$GREEN" "✓ $test_name passed"
            return 0
        else
            print_status "$RED" "✗ $test_name failed"
            return 1
        fi
    else
        # In quiet mode, capture output and only show on failure
        local output
        if output=$(eval "$command" 2>&1); then
            print_status "$GREEN" "✓ $test_name passed"
            return 0
        else
            print_status "$RED" "✗ $test_name failed"
            echo "$output"  # Show output only on failure
            return 1
        fi
    fi
}

# Clean build directory for fresh start
print_status "$YELLOW" "=== Preparing test environment ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake for testing
print_status "$YELLOW" "=== Configuring build ==="
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON -DENABLE_DEBUG_LOGGING=ON"

# Add coverage flags if requested
if [ "$ENABLE_COVERAGE" -eq 1 ]; then
    CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_CXX_FLAGS='--coverage' -DCMAKE_EXE_LINKER_FLAGS='--coverage'"
    print_status "$BLUE" "Coverage analysis enabled"
fi

# Add sanitizer flags for detecting memory issues
CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_CXX_FLAGS='-fsanitize=address -fsanitize=undefined'"
CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address -fsanitize=undefined'"

cmake $CMAKE_FLAGS ..

# Build the project
print_status "$YELLOW" "=== Building test binaries ==="
make -j$(nproc)

# Run unit tests
print_status "$YELLOW" "=== Running unit tests ==="
TEST_FAILED=0

# Basic unit test execution
if ! run_test "Unit tests" "./vsocky_test"; then
    TEST_FAILED=1
fi

# Run tests with different configurations to catch edge cases
print_status "$YELLOW" "=== Running configuration tests ==="

# Test with minimal buffers (catches buffer overflow issues)
if ! run_test "Minimal buffer test" "VSOCKY_BUFFER_SIZE=1024 ./vsocky_test"; then
    TEST_FAILED=1
fi

# Test with large inputs (catches integer overflow issues)
if ! run_test "Large input test" "VSOCKY_MAX_CODE_SIZE=1048576 ./vsocky_test"; then
    TEST_FAILED=1
fi

# Memory leak detection with Valgrind (if enabled)
if [ "$ENABLE_VALGRIND" -eq 1 ]; then
    print_status "$YELLOW" "=== Running memory checks ==="
    
    # Check for memory leaks
    if command -v valgrind >/dev/null 2>&1; then
        if ! run_test "Memory leak check" "valgrind --leak-check=full --error-exitcode=1 ./vsocky_test"; then
            TEST_FAILED=1
        fi
        
        # Check for uninitialized memory usage
        if ! run_test "Uninitialized memory check" "valgrind --track-origins=yes --error-exitcode=1 ./vsocky_test"; then
            TEST_FAILED=1
        fi
    else
        print_status "$YELLOW" "Valgrind not found, skipping memory checks"
    fi
fi

# Performance regression tests
print_status "$YELLOW" "=== Running performance tests ==="

# Build a simple performance test if it exists
if [ -f "../benchmarks/bench_protocol.cpp" ]; then
    # Build benchmark separately
    g++ -O3 -std=c++23 ../benchmarks/bench_protocol.cpp -I../include -o bench_protocol
    
    # Run and check if performance meets targets
    if ! run_test "Protocol parsing performance" "./bench_protocol"; then
        print_status "$YELLOW" "Performance regression detected"
        # Don't fail on performance regression, just warn
    fi
fi

# Integration tests with mock vsocket
print_status "$YELLOW" "=== Running integration tests ==="

# Create a test scenario that simulates real usage
cat > test_scenario.json << 'EOF'
{
    "request_id": "test-123",
    "language": "python",
    "code": "print('Hello, World!')",
    "timeout": 5000,
    "memory_limit": 67108864
}
EOF

# Test the full pipeline with mock vsocket
if [ -f "./vsocky" ]; then
    # Start vsocky in test mode (listening on Unix socket instead of vsocket)
    if ! run_test "Integration test" "timeout 10 ./vsocky --test-mode < test_scenario.json"; then
        TEST_FAILED=1
    fi
fi

# Stress tests to find race conditions and resource leaks
print_status "$YELLOW" "=== Running stress tests ==="

# Create a stress test script
cat > stress_test.sh << 'EOF'
#!/bin/bash
# Run multiple instances to catch concurrency issues
for i in {1..10}; do
    ./vsocky_test --gtest_repeat=10 --gtest_shuffle &
done
wait
EOF

chmod +x stress_test.sh
if ! run_test "Stress test" "./stress_test.sh"; then
    TEST_FAILED=1
fi

# Fuzzing tests (if fuzzer is available)
if command -v afl-fuzz >/dev/null 2>&1; then
    print_status "$YELLOW" "=== Running fuzz tests ==="
    # This is just a placeholder - real fuzzing setup is more complex
    print_status "$BLUE" "Fuzzing available but not configured"
fi

# Code coverage report (if enabled)
if [ "$ENABLE_COVERAGE" -eq 1 ]; then
    print_status "$YELLOW" "=== Generating coverage report ==="
    
    # Create coverage directory
    mkdir -p "../$COVERAGE_DIR"
    
    # Generate coverage info
    if command -v lcov >/dev/null 2>&1; then
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --remove coverage.info '*/test/*' --output-file coverage.info
        
        # Generate HTML report
        genhtml coverage.info --output-directory "../$COVERAGE_DIR"
        
        # Show summary
        lcov --summary coverage.info
        print_status "$GREEN" "Coverage report generated in $COVERAGE_DIR/"
    else
        print_status "$YELLOW" "lcov not found, skipping coverage report"
    fi
fi

# Static analysis
print_status "$YELLOW" "=== Running static analysis ==="

# cppcheck for common issues
if command -v cppcheck >/dev/null 2>&1; then
    if ! run_test "Static analysis (cppcheck)" "cppcheck --enable=all --error-exitcode=1 --suppress=missingIncludeSystem ../src/"; then
        TEST_FAILED=1
    fi
else
    print_status "$YELLOW" "cppcheck not found, skipping static analysis"
fi

# clang-tidy for more advanced checks
if command -v clang-tidy >/dev/null 2>&1; then
    # Create a simple .clang-tidy config if it doesn't exist
    if [ ! -f "../.clang-tidy" ]; then
        cat > ../.clang-tidy << 'EOF'
Checks: 'clang-analyzer-*,performance-*,portability-*,modernize-*'
WarningsAsErrors: 'clang-analyzer-*'
EOF
    fi
    
    # Run clang-tidy on source files
    find ../src -name "*.cpp" -not -path "*/test/*" | while read -r file; do
        if ! run_test "Linting $file" "clang-tidy $file -- -std=c++23 -I../include"; then
            TEST_FAILED=1
        fi
    done
else
    print_status "$YELLOW" "clang-tidy not found, skipping advanced static analysis"
fi

# Summary
print_status "$YELLOW" "=== Test Summary ==="
if [ $TEST_FAILED -eq 0 ]; then
    print_status "$GREEN" "All tests passed! 🎉"
    
    # Show binary size for monitoring bloat
    if [ -f "./vsocky" ]; then
        print_status "$BLUE" "Binary size: $(ls -lh ./vsocky | awk '{print $5}')"
    fi
else
    print_status "$RED" "Some tests failed! ❌"
    print_status "$YELLOW" "Run with VERBOSE=1 for detailed output"
fi

# Cleanup
rm -f test_scenario.json stress_test.sh

# Return appropriate exit code
exit $TEST_FAILED