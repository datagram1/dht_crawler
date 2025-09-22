cmake_minimum_required(VERSION 3.16)

# Integration tests executable
add_executable(integration_tests
    test_dht_crawler_integration.cpp
    test_mysql_integration.cpp
    test_metadata_fetching.cpp
    test_main.cpp
)

# Link with Google Test only (tests are self-contained)
target_link_libraries(integration_tests
    GTest::gtest
    GTest::gtest_main
)

# Include directories
target_include_directories(integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LIBTORRENT_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIRS}
)

# Compiler definitions for testing
target_compile_definitions(integration_tests PRIVATE
    TESTING_MODE=1
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# Compiler flags
target_compile_options(integration_tests PRIVATE ${LIBTORRENT_CFLAGS_OTHER})

# Add test discovery
include(GoogleTest)
gtest_discover_tests(integration_tests)
