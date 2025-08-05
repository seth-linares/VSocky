# Tests for VSocky

# Utility tests
add_executable(test_utils 
    utils/test_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/signal_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/base64.cpp
)

target_include_directories(test_utils PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(test_utils PRIVATE cxx_std_23)

# Link pthread for signal handling
target_link_libraries(test_utils PRIVATE pthread)

# Make test runnable with ctest
add_test(NAME test_utils COMMAND test_utils)

# Add more tests as we implement components
# TODO: test_vsocket, test_protocol, etc.