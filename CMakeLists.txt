cmake_minimum_required(VERSION 3.28.3)
project(vsocky VERSION 0.1.0 LANGUAGES CXX)

cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_STATIC "Build static binary" ON)
option(USE_SIMDJSON "Use simdjson for JSON parsing" ON)
option(BUILD_TESTS "Build tests" ON)

# Include our compiler flags
include(cmake/CompilerFlags.cmake)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find packages
include(FetchContent)

# Fetch simdjson with proper configuration
if(USE_SIMDJSON)
    # CRITICAL: Configure simdjson before fetching to avoid architecture issues
    # Use new non-deprecated options (simdjson v3.10.0+)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # For static library

    # Suppress warnings from simdjson
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)
    
    # Disable simdjson tests and tools to speed up build
    set(SIMDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    
    # Temporarily remove any global architecture flags for simdjson
    set(ORIGINAL_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    set(ORIGINAL_C_FLAGS ${CMAKE_C_FLAGS})
    
    # Remove problematic flags if they exist
    string(REGEX REPLACE "-march=[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "-mtune=[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "-mavx[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "-msse[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG v3.13.0  # Latest stable - uses new CMake options
        GIT_SHALLOW TRUE
    )
    
    FetchContent_MakeAvailable(simdjson)
    
    # Restore original flags
    set(CMAKE_CXX_FLAGS ${ORIGINAL_CXX_FLAGS})
    set(CMAKE_C_FLAGS ${ORIGINAL_C_FLAGS})
endif()

# Collect source files
set(VSOCKY_SOURCES
    src/main.cpp
    
    # Utilities
    src/utils/signal_handler.cpp
    src/utils/base64.cpp
    
    # VSock Socket Layer
    src/vsocket/connection.cpp
    
    # TODO: Add these as we implement them
    # src/vsocket/vsock_server.cpp
    # src/vsocket/message_framer.cpp
    # src/protocol/request.cpp
    # src/protocol/response.cpp
    # src/protocol/handler.cpp
)

# Create executable
add_executable(vsocky ${VSOCKY_SOURCES})

# Apply architecture optimization to OUR target only
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Size optimization for Alpine static builds
    if(BUILD_STATIC)
        target_compile_options(vsocky PRIVATE
            -Os  # Optimize for size instead of -O3
            -fno-asynchronous-unwind-tables
            -fno-ident
            -ffunction-sections
            -fdata-sections
            -fmerge-all-constants
            -fno-unwind-tables
        )
        
        # More aggressive linker optimizations
        target_link_options(vsocky PRIVATE
            -Wl,--as-needed
            -Wl,--gc-sections
            -Wl,--strip-all
            -Wl,--build-id=none  # Remove build ID to save space
            -Wl,-z,norelro       # Save ~10KB
            -Wl,--hash-style=gnu # Smaller than sysv
        )
    else()
        # Regular optimization for non-static builds
        target_compile_options(vsocky PRIVATE
            -O3
            -march=x86-64-v2
            -mtune=generic
        )
    endif()
endif()

# Link libraries with warning suppression for simdjson
if(USE_SIMDJSON)
    # Create an interface target to suppress warnings from simdjson
    add_library(simdjson_wrapper INTERFACE)
    target_link_libraries(simdjson_wrapper INTERFACE simdjson)
    
    # Suppress problematic warnings when including simdjson headers
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(simdjson_wrapper INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-conversion>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-sign-conversion>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>
        )
    endif()
    
    target_link_libraries(vsocky PRIVATE simdjson_wrapper)
    target_compile_definitions(vsocky PRIVATE HAS_SIMDJSON=1)
endif()

# Static linking if requested
if(BUILD_STATIC)
    set_target_properties(vsocky PROPERTIES 
        LINK_FLAGS "-static"
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON
    )
    
    # More aggressive static linking flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(vsocky PRIVATE 
            -static-libgcc 
            -static-libstdc++
            -Wl,--whole-archive -lpthread -Wl,--no-whole-archive  # Ensure pthread is fully linked
        )
    endif()
endif()

# =============================================================================
# TESTING SUPPORT
# =============================================================================
if(BUILD_TESTS)
    # Enable testing functionality
    enable_testing()
    
    # Add the tests subdirectory
    # This will process tests/CMakeLists.txt which defines test targets
    add_subdirectory(tests)
    
    # Create a convenience target to build all tests
    # Note: This depends on specific test executables, so it must come AFTER add_subdirectory(tests)
    add_custom_target(build_tests
        DEPENDS test_utils test_connection  # Add more test executables as we create them
        COMMENT "Building all tests"
    )
    
    # Create a target to build and run all tests with nice output
    # This is the main target developers should use during development
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS build_tests
        COMMENT "Building and running all tests"
    )
    
    # Note: test_verbose, test_unit, and test_valgrind targets are defined in tests/CMakeLists.txt
    # We don't duplicate them here to avoid naming conflicts
endif()

# Install target
install(TARGETS vsocky DESTINATION bin)

# Print configuration
message(STATUS "VSocky Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Static Build: ${BUILD_STATIC}")
message(STATUS "  Using simdjson: ${USE_SIMDJSON}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Add a custom target to show binary size
add_custom_target(size
    COMMAND size "$<TARGET_FILE:vsocky>"
    DEPENDS vsocky
    COMMENT "Binary size information"
)