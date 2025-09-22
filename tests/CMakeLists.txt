cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find Google Test
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/tests)

# Add test executables
add_subdirectory(unit)
add_subdirectory(integration)

# Add custom test targets
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS unit_tests integration_tests
    COMMENT "Running all tests"
)
