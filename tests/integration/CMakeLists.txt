cmake_minimum_required(VERSION 3.16)

# Integration tests executable
add_executable(integration_tests
    test_dht_crawler_integration.cpp
    test_mysql_integration.cpp
    test_metadata_fetching.cpp
    test_main.cpp
)

# Link with Google Test only (tests are self-contained)
# Force static linking for macOS
if(APPLE)
    target_link_libraries(integration_tests
        /opt/homebrew/lib/libgtest.a
        /opt/homebrew/lib/libgtest_main.a
    )
else()
    target_link_libraries(integration_tests
        GTest::gtest
        GTest::gtest_main
    )
endif()

# Include directories
target_include_directories(integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LIBTORRENT_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIRS}
    /opt/homebrew/include
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
