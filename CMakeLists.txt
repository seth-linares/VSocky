cmake_minimum_required(VERSION 3.28.3)
project(vsocky VERSION 1.0 LANGUAGES CXX)

# Enforce C++23 standard - we want the latest performance features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_EXTENSIONS OFF)  # Ensure portability

# Default to Release build for optimization
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific flags for maximum performance
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Base optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    
    # Architecture-specific optimization (careful with this in VMs)
    # For now, we'll use conservative optimizations that work everywhere
    add_compile_options(-march=x86-64-v2)  # Baseline modern x86-64
    
    # Link-time optimization for smaller, faster binary
    add_compile_options(-flto)
    add_link_options(-flto)
    
    # Remove features we don't need for smaller binary
    add_compile_options(-fno-exceptions)  # We'll use error codes
    add_compile_options(-fno-rtti)       # No runtime type information
    
    # Optimize for size as well (helps with VM startup)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections)
    
    # Security hardening (with minimal performance impact)
    add_compile_options(-D_FORTIFY_SOURCE=2)
    add_compile_options(-fstack-protector-strong)
    add_link_options(-Wl,-z,relro -Wl,-z,now)
    
    # Warnings to catch issues early
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Werror)  # Treat warnings as errors
    
    # Static linking for standalone binary
    add_link_options(-static)
    
    # Strip symbols in release mode
    add_link_options(-s)
endif()

# Define build options
option(ENABLE_TESTING "Enable test builds" ON)
option(ENABLE_PROFILING "Enable profiling support" OFF)
option(ENABLE_DEBUG_LOGGING "Enable debug logging" OFF)

# Configure compile definitions based on options
if(ENABLE_DEBUG_LOGGING)
    add_compile_definitions(DEBUG_LOGGING)
endif()

if(ENABLE_PROFILING)
    add_compile_options(-pg)
    add_link_options(-pg)
endif()

# Include directories
include_directories(include)

# Main executable
add_executable(vsocky 
    src/main.cpp
    src/vsocket.cpp
    src/protocol.cpp
    src/executor.cpp
    src/monitor.cpp
)

# Test executable (only if testing enabled)
if(ENABLE_TESTING)
    add_executable(vsocky_test
        src/test/test_main.cpp
        src/test/test_protocol.cpp
        src/test/test_executor.cpp
        src/test/mock_vsocket.cpp
        # Link same sources but exclude main.cpp
        src/vsocket.cpp
        src/protocol.cpp
        src/executor.cpp
        src/monitor.cpp
    )
    
    # Enable testing
    enable_testing()
    add_test(NAME vsocky_test COMMAND vsocky_test)
endif()

# Installation rules
install(TARGETS vsocky
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "vsocky configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "  Testing enabled: ${ENABLE_TESTING}")
message(STATUS "  Profiling enabled: ${ENABLE_PROFILING}")
message(STATUS "  Debug logging: ${ENABLE_DEBUG_LOGGING}")
message(STATUS "")