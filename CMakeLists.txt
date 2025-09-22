cmake_minimum_required(VERSION 3.16)
project(DHTCrawler VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatic version increment based on build number
# This will increment the patch version for each build
file(READ "${CMAKE_SOURCE_DIR}/build_number.txt" BUILD_NUMBER_CONTENT)
if(BUILD_NUMBER_CONTENT)
    string(STRIP "${BUILD_NUMBER_CONTENT}" CURRENT_BUILD_NUMBER)
    math(EXPR CURRENT_BUILD_NUMBER "${CURRENT_BUILD_NUMBER} + 1")
else()
    set(CURRENT_BUILD_NUMBER "1")
endif()

# Write the incremented build number back
file(WRITE "${CMAKE_SOURCE_DIR}/build_number.txt" "${CURRENT_BUILD_NUMBER}")

# Set version with build number
set(PROJECT_VERSION "${PROJECT_VERSION}.${CURRENT_BUILD_NUMBER}")
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH ${CURRENT_BUILD_NUMBER})

# Print version information
message(STATUS "Building DHT Crawler version ${PROJECT_VERSION}")

# Platform detection - allow override via CMAKE_TOOLCHAIN_FILE or explicit variables
if(NOT DEFINED FORCE_PLATFORM_NAME)
    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(PLATFORM_NAME "macos")
            set(ARCHITECTURE "arm")
            set(PLATFORM_DIR "macos/arm")
        else()
            set(PLATFORM_NAME "macos")
            set(ARCHITECTURE "x86")
            set(PLATFORM_DIR "macos/x86")
        endif()
    elseif(WIN32)
        set(PLATFORM_NAME "windows")
        set(ARCHITECTURE "x86")
        set(PLATFORM_DIR "windows/x86")
    elseif(UNIX)
        set(PLATFORM_NAME "linux")
        set(ARCHITECTURE "x86")
        set(PLATFORM_DIR "linux/x86")
    else()
        set(PLATFORM_NAME "unknown")
        set(ARCHITECTURE "unknown")
        set(PLATFORM_DIR "unknown")
    endif()
else()
    # Use forced platform settings
    set(PLATFORM_NAME ${FORCE_PLATFORM_NAME})
    set(ARCHITECTURE ${FORCE_ARCHITECTURE})
    set(PLATFORM_DIR ${FORCE_PLATFORM_DIR})
    
    # Handle cross-compilation for macOS x86 on Apple Silicon
    if(APPLE AND FORCE_ARCHITECTURE STREQUAL "x86")
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
        message(STATUS "Cross-compiling for macOS x86_64")
        
        # For cross-compilation, we need to disable linking to ARM64 libraries
        # and potentially skip some dependencies that aren't available for x86_64
        set(CROSS_COMPILING_X86 TRUE)
    endif()
endif()

message(STATUS "Detected platform: ${PLATFORM_NAME} (${ARCHITECTURE})")
message(STATUS "Build directory: ${PLATFORM_DIR}")

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/builds/${PLATFORM_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/builds/${PLATFORM_DIR}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/builds/${PLATFORM_DIR}/$<CONFIG>")

# Build configurations
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build configurations" FORCE)
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Find libtorrent
if(WIN32 OR FORCE_PLATFORM_NAME STREQUAL "windows")
    # For Windows cross-compilation, skip libtorrent for now
    message(WARNING "Skipping libtorrent for Windows cross-compilation")
    set(LIBTORRENT_FOUND FALSE)
    set(LIBTORRENT_LIBRARIES "")
    set(LIBTORRENT_INCLUDE_DIRS "")
    add_definitions(-DDISABLE_LIBTORRENT)
elseif(CROSS_COMPILING_X86)
    # For macOS x86 cross-compilation, skip libtorrent for now
    # as it's not available for x86_64 on Apple Silicon
    message(WARNING "Skipping libtorrent for macOS x86 cross-compilation")
    set(LIBTORRENT_FOUND FALSE)
    set(LIBTORRENT_LIBRARIES "")
    set(LIBTORRENT_INCLUDE_DIRS "")
    add_definitions(-DDISABLE_LIBTORRENT)
else()
    pkg_check_modules(LIBTORRENT REQUIRED libtorrent-rasterbar)
    
    # Fallback: if pkg-config fails, try to find libtorrent manually
    if(NOT LIBTORRENT_FOUND)
        find_library(LIBTORRENT_LIBRARY NAMES torrent-rasterbar libtorrent-rasterbar)
        find_path(LIBTORRENT_INCLUDE_DIR libtorrent/session.hpp)
        if(LIBTORRENT_LIBRARY AND LIBTORRENT_INCLUDE_DIR)
            set(LIBTORRENT_FOUND TRUE)
            set(LIBTORRENT_LIBRARIES ${LIBTORRENT_LIBRARY})
            set(LIBTORRENT_INCLUDE_DIRS ${LIBTORRENT_INCLUDE_DIR})
        endif()
    endif()
endif()

# Find MySQL libraries directly
if(CROSS_COMPILING_X86)
    # For macOS x86 cross-compilation, skip MySQL for now
    # as it's not available for x86_64 on Apple Silicon
    message(WARNING "Skipping MySQL for macOS x86 cross-compilation")
    set(MYSQL_FOUND FALSE)
    set(MYSQL_LIBRARIES "")
    set(MYSQL_INCLUDE_DIRS "")
    add_definitions(-DDISABLE_MYSQL)
else()
    find_library(MYSQL_LIBRARY NAMES mysqlclient libmysqlclient)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
    if(MYSQL_LIBRARY AND MYSQL_INCLUDE_DIR)
        set(MYSQL_FOUND TRUE)
        set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
        set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
    else()
        if(WIN32 OR FORCE_PLATFORM_NAME STREQUAL "windows")
            message(WARNING "MySQL library not found. Building without MySQL support for Windows.")
            set(MYSQL_FOUND FALSE)
            set(MYSQL_LIBRARIES "")
            set(MYSQL_INCLUDE_DIRS "")
            add_definitions(-DDISABLE_MYSQL)
        else()
            message(FATAL_ERROR "MySQL library not found. Please install MySQL development libraries.")
        endif()
    endif()
endif()

# Platform-specific library finding
message(STATUS "Platform detection debug:")
message(STATUS "  APPLE: ${APPLE}")
message(STATUS "  WIN32: ${WIN32}")
message(STATUS "  UNIX: ${UNIX}")
message(STATUS "  FORCE_PLATFORM_NAME: ${FORCE_PLATFORM_NAME}")

if(APPLE AND NOT FORCE_PLATFORM_NAME STREQUAL "linux")
    # macOS specific setup
    message(STATUS "Using macOS platform libraries")
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(SYSTEM_CONFIGURATION SystemConfiguration)
    find_library(SECURITY Security)
    
    set(PLATFORM_LIBS ${CORE_FOUNDATION} ${SYSTEM_CONFIGURATION} ${SECURITY})
    
elseif(WIN32 AND NOT FORCE_PLATFORM_NAME STREQUAL "linux")
    # Windows specific setup
    message(STATUS "Using Windows platform libraries")
    set(PLATFORM_LIBS ws2_32 wsock32)
    
elseif(UNIX OR FORCE_PLATFORM_NAME STREQUAL "linux")
    # Linux specific setup
    message(STATUS "Using Linux platform libraries")
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
    message(STATUS "PLATFORM_LIBS set to: ${PLATFORM_LIBS}")
else()
    message(WARNING "No platform libraries configured")
endif()

# Include directories
if(LIBTORRENT_FOUND)
    include_directories(${LIBTORRENT_INCLUDE_DIRS})
endif()
if(MYSQL_FOUND)
    include_directories(${MYSQL_INCLUDE_DIRS})
endif()

# Source files
set(SOURCES
    src/dht_crawler.cpp
)

# Header files
set(HEADERS
    src/enhanced_metadata_manager.hpp
    src/concurrent_dht_manager.hpp
)

# Create executable
add_executable(dht_crawler ${SOURCES} ${HEADERS})

# Link libraries
if(LIBTORRENT_FOUND AND MYSQL_FOUND)
    target_link_libraries(dht_crawler 
        ${LIBTORRENT_LIBRARIES}
        ${MYSQL_LIBRARIES}
        ${PLATFORM_LIBS}
    )
elseif(LIBTORRENT_FOUND)
    target_link_libraries(dht_crawler 
        ${LIBTORRENT_LIBRARIES}
        ${PLATFORM_LIBS}
    )
elseif(MYSQL_FOUND)
    target_link_libraries(dht_crawler 
        ${MYSQL_LIBRARIES}
        ${PLATFORM_LIBS}
    )
else()
    target_link_libraries(dht_crawler 
        ${PLATFORM_LIBS}
    )
endif()

# Add library search paths (only for native builds, not cross-compilation)
if(NOT CROSS_COMPILING_X86)
    target_link_directories(dht_crawler PRIVATE /opt/homebrew/lib)
endif()

# Compiler definitions
target_compile_definitions(dht_crawler PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
    PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    BUILD_NUMBER=${CURRENT_BUILD_NUMBER}
    PLATFORM_NAME="${PLATFORM_NAME}"
)

# Compiler flags
target_compile_options(dht_crawler PRIVATE ${LIBTORRENT_CFLAGS_OTHER})

# Create version header
configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_BINARY_DIR}/version.h"
    @ONLY
)

# Include version header
target_include_directories(dht_crawler PRIVATE ${CMAKE_BINARY_DIR})

# =============================================================================
# PACKAGE CONFIGURATION
# =============================================================================

# Include CMake package configuration
include(CMakePackageConfigHelpers)

# Create package configuration file (only if PLATFORM_LIBS is defined)
if(DEFINED PLATFORM_LIBS)
    configure_package_config_file(
        "${CMAKE_SOURCE_DIR}/cmake/DHTCrawlerConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/DHTCrawlerConfig.cmake"
        INSTALL_DESTINATION lib/cmake/DHTCrawler
        PATH_VARS
            LIBTORRENT_INCLUDE_DIRS
            MYSQL_INCLUDE_DIRS
            LIBTORRENT_LIBRARIES
            MYSQL_LIBRARIES
            PLATFORM_LIBS
    )
else()
    message(WARNING "PLATFORM_LIBS not defined, skipping package configuration")
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/DHTCrawlerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Installation
install(TARGETS dht_crawler
    EXPORT DHTCrawlerTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install package configuration files
install(FILES
    "${CMAKE_BINARY_DIR}/DHTCrawlerConfig.cmake"
    "${CMAKE_BINARY_DIR}/DHTCrawlerConfigVersion.cmake"
    DESTINATION lib/cmake/DHTCrawler
)

# Install export targets
install(EXPORT DHTCrawlerTargets
    FILE DHTCrawlerTargets.cmake
    NAMESPACE DHTCrawler::
    DESTINATION lib/cmake/DHTCrawler
)

# =============================================================================
# TESTING CONFIGURATION
# =============================================================================

# Enable testing if requested
option(ENABLE_TESTING "Enable testing" OFF)

if(ENABLE_TESTING)
    # Find Google Test
    find_package(GTest QUIET)
    
    if(GTest_FOUND)
        message(STATUS "Google Test found - enabling tests")
        enable_testing()
        add_subdirectory(tests)
    else()
        message(WARNING "Google Test not found - tests disabled")
        message(STATUS "To enable tests, install Google Test:")
        message(STATUS "  macOS: brew install googletest")
        message(STATUS "  Linux: sudo apt install libgtest-dev")
    endif()
else()
    message(STATUS "Testing disabled - use -DENABLE_TESTING=ON to enable")
endif()

# Print build information
message(STATUS "=== Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${PLATFORM_NAME}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "LibTorrent: ${LIBTORRENT_VERSION}")
message(STATUS "MySQL: ${MYSQL_VERSION}")
message(STATUS "Package Config: ${CMAKE_BINARY_DIR}/DHTCrawlerConfig.cmake")
message(STATUS "Testing: ${ENABLE_TESTING}")
message(STATUS "========================")