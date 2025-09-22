cmake_minimum_required(VERSION 3.16)

# Unit tests executable
add_executable(unit_tests
    test_metadata_manager.cpp
    test_concurrent_dht_manager.cpp
    test_mysql_connection.cpp
    test_hash_conversion.cpp
    test_torrent_discovery.cpp
    test_main.cpp
)

# Link with Google Test only (tests are self-contained)
target_link_libraries(unit_tests
    GTest::gtest
    GTest::gtest_main
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LIBTORRENT_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIRS}
)

# Compiler definitions for testing
target_compile_definitions(unit_tests PRIVATE
    TESTING_MODE=1
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# Compiler flags
target_compile_options(unit_tests PRIVATE ${LIBTORRENT_CFLAGS_OTHER})

# Add test discovery
include(GoogleTest)
gtest_discover_tests(unit_tests)
