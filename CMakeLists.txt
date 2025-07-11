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

# Include our compiler flags (now without architecture flags)
include(cmake/CompilerFlags.cmake)

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

# Create executable
add_executable(vsocky src/main.cpp)

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

# Install target
install(TARGETS vsocky DESTINATION bin)

# Print configuration
message(STATUS "VSocky Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Static Build: ${BUILD_STATIC}")
message(STATUS "  Using simdjson: ${USE_SIMDJSON}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Add a custom target to show binary size
add_custom_target(size
    COMMAND size "$<TARGET_FILE:vsocky>"
    DEPENDS vsocky
    COMMENT "Binary size information"
)