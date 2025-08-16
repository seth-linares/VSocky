# =============================================================================
# VSocky Test Suite Configuration
# =============================================================================
# This CMake file configures all unit tests for the VSocky project.
# Each component gets its own test executable to keep tests isolated and
# make it easy to run specific test suites during development.
# =============================================================================

# Enable CTest for running tests
enable_testing()

# =============================================================================
# COMMON TEST CONFIGURATION
# =============================================================================
# Instead of repeating the same settings for each test, we create a helper
# function that applies common configuration to all test targets.

function(add_vsocky_test TEST_NAME)
    # Parse additional source files if provided
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create the test executable
    add_executable(${TEST_NAME} ${ARG_SOURCES})
    
    # Common configuration for all tests
    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Set C++ standard
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)
    
    # Apply same warning flags as main project (but maybe relax some for tests)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TEST_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter  # Tests might have unused params
            -Wno-unused-variable   # Tests might have verification variables
        )
    endif()
    
    # Link dependencies
    if(ARG_DEPENDENCIES)
        target_link_libraries(${TEST_NAME} PRIVATE ${ARG_DEPENDENCIES})
    endif()
    
    # Standard libraries that most tests need
    target_link_libraries(${TEST_NAME} PRIVATE pthread)
    
    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 10  # Each test should complete within 10 seconds
        LABELS "unit"  # Label for filtering tests
    )
endfunction()

# =============================================================================
# UTILITY TESTS
# =============================================================================
# Tests for the utility components (error handling, signal handler, base64)

add_vsocky_test(test_utils
    SOURCES 
        utils/test_utils.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/signal_handler.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/base64.cpp
)

# =============================================================================
# VSOCKET TESTS
# =============================================================================
# Tests for the VSock socket layer components

# Connection class tests
add_vsocky_test(test_connection
    SOURCES
        vsocket/test_connection.cpp
        ${CMAKE_SOURCE_DIR}/src/vsocket/connection.cpp
        # Note: connection.cpp might need error.hpp, but that's header-only
)

# Future: MessageFramer tests (when implemented)
# add_vsocky_test(test_message_framer
#     SOURCES
#         vsocket/test_message_framer.cpp
#         ${CMAKE_SOURCE_DIR}/src/vsocket/message_framer.cpp
#         ${CMAKE_SOURCE_DIR}/src/vsocket/connection.cpp
# )

# Future: VSockServer tests (when implemented)
# add_vsocky_test(test_vsock_server
#     SOURCES
#         vsocket/test_vsock_server.cpp
#         ${CMAKE_SOURCE_DIR}/src/vsocket/vsock_server.cpp
#         ${CMAKE_SOURCE_DIR}/src/vsocket/connection.cpp
#         ${CMAKE_SOURCE_DIR}/src/vsocket/message_framer.cpp
# )

# =============================================================================
# PROTOCOL TESTS (Future)
# =============================================================================
# Tests for the protocol layer components

# add_vsocky_test(test_protocol
#     SOURCES
#         protocol/test_protocol.cpp
#         ${CMAKE_SOURCE_DIR}/src/protocol/request.cpp
#         ${CMAKE_SOURCE_DIR}/src/protocol/response.cpp
#         ${CMAKE_SOURCE_DIR}/src/protocol/handler.cpp
#     DEPENDENCIES
#         simdjson  # Protocol tests will need JSON parsing
# )

# =============================================================================
# INTEGRATION TESTS
# =============================================================================
# Higher-level tests that test multiple components together

# Future: End-to-end test with real VSock communication
# add_vsocky_test(test_integration
#     SOURCES
#         integration/test_end_to_end.cpp
#         # Would include most source files
#     DEPENDENCIES
#         simdjson
# )

# =============================================================================
# TEST UTILITIES
# =============================================================================
# If we had common test utilities, we could create a library for them

# add_library(test_helpers STATIC
#     common/test_helpers.cpp
#     common/mock_connection.cpp
# )
# target_include_directories(test_helpers PUBLIC ${CMAKE_SOURCE_DIR}/include)
# target_compile_features(test_helpers PUBLIC cxx_std_23)

# =============================================================================
# CUSTOM TARGETS FOR CONVENIENCE
# =============================================================================

# Run all unit tests
add_custom_target(test_unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_utils test_connection
    COMMENT "Running unit tests"
)

# Run tests with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    DEPENDS test_utils test_connection
    COMMENT "Running all tests with verbose output"
)

# Run tests under valgrind (if available)
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
    add_custom_target(test_valgrind
        COMMAND ${CMAKE_CTEST_COMMAND} 
            --overwrite MemoryCheckCommand=${VALGRIND_EXECUTABLE}
            --overwrite MemoryCheckCommandOptions="--leak-check=full --error-exitcode=1"
            -T memcheck
        DEPENDS test_utils test_connection
        COMMENT "Running tests under valgrind"
    )
endif()

# =============================================================================
# CONFIGURATION SUMMARY
# =============================================================================
message(STATUS "Tests Configuration:")
message(STATUS "  Test directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Number of test suites: 2")  # Update as we add more
if(VALGRIND_EXECUTABLE)
    message(STATUS "  Valgrind support: ENABLED")
else()
    message(STATUS "  Valgrind support: DISABLED (valgrind not found)")
endif()