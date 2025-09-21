#!/bin/bash

# DHT Crawler Release Build Script
# This script builds all release configurations simultaneously

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="DHTCrawler"
BUILD_DIR="builds"
BUILD_CONFIGS=("Debug" "Release")
PLATFORMS=("macos/arm" "macos/x86" "windows/x86" "linux/x86")

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to detect current platform
detect_platform() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [[ $(uname -m) == "arm64" ]]; then
            echo "macos/arm"
        else
            echo "macos/x86"
        fi
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux/x86"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        echo "windows/x86"
    else
        echo "unknown"
    fi
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is not installed. Please install CMake first."
        exit 1
    fi
    
    # Check pkg-config
    if ! command -v pkg-config &> /dev/null; then
        print_error "pkg-config is not installed. Please install pkg-config first."
        exit 1
    fi
    
    # Check libtorrent
    if ! pkg-config --exists libtorrent-rasterbar; then
        print_error "libtorrent-rasterbar is not installed. Please install libtorrent first."
        exit 1
    fi
    
    # Check MySQL client
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if ! brew list mysql-client &> /dev/null; then
            print_warning "MySQL client not found via Homebrew. Make sure MySQL development libraries are installed."
        fi
    fi
    
    print_success "Dependencies check completed"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_status "Build directory does not exist, skipping clean"
    fi
}

# Function to create build directory structure
create_build_structure() {
    print_status "Creating build directory structure..."
    
    mkdir -p "$BUILD_DIR"
    
    # Create platform directories
    for platform in "${PLATFORMS[@]}"; do
        for config in "${BUILD_CONFIGS[@]}"; do
            mkdir -p "$BUILD_DIR/$platform/$config"
        done
    done
    
    print_success "Build structure created"
}

# Function to build for a specific platform and configuration
build_platform_config() {
    local platform=$1
    local config=$2
    local build_path="$BUILD_DIR/$platform/$config"
    
    print_status "Building $platform/$config..."
    
    # Configure CMake
    cmake -S . -B "$build_path" \
        -DCMAKE_BUILD_TYPE="$config" \
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$BUILD_DIR/$platform/$config" \
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$BUILD_DIR/$platform/$config" \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="$BUILD_DIR/$platform/$config"
    
    # Build
    cmake --build "$build_path" --config "$config" --parallel
    
    print_success "Built $platform/$config"
}

# Function to build all configurations for current platform
build_current_platform() {
    local current_platform=$(detect_platform)
    print_status "Building for current platform: $current_platform"
    
    for config in "${BUILD_CONFIGS[@]}"; do
        build_platform_config "$current_platform" "$config"
    done
}

# Function to build all platforms (cross-compilation simulation)
build_all_platforms() {
    print_status "Building all platform configurations..."
    
    # Build for current platform
    build_current_platform
    
    # Note: Cross-compilation would require additional setup
    # For now, we'll build what's possible on the current platform
    print_warning "Cross-platform builds require additional toolchain setup"
    print_status "Built configurations for current platform: $(detect_platform)"
}

# Function to create package
create_package() {
    print_status "Creating CMake package..."
    
    local current_platform=$(detect_platform)
    local package_dir="$BUILD_DIR/package"
    
    mkdir -p "$package_dir"
    
    # Install package files
    for config in "${RELEASE_CONFIGS[@]}"; do
        local build_path="$BUILD_DIR/$current_platform/$config"
        if [ -d "$build_path" ]; then
            cmake --install "$build_path" --prefix "$package_dir/$config"
        fi
    done
    
    print_success "Package created in $package_dir"
}

# Function to run tests
run_tests() {
    print_status "Running tests..."
    
    local current_platform=$(detect_platform)
    
    for config in "${RELEASE_CONFIGS[@]}"; do
        local build_path="$BUILD_DIR/$current_platform/$config"
        local executable="$BUILD_DIR/$current_platform/$config/dht_crawler"
        
        if [ -f "$executable" ]; then
            print_status "Testing $config build..."
            if "$executable" --help > /dev/null 2>&1; then
                print_success "Test passed for $config"
            else
                print_error "Test failed for $config"
            fi
        fi
    done
}

# Function to show build summary
show_summary() {
    print_status "Build Summary:"
    echo "=================="
    
    local current_platform=$(detect_platform)
    
    for config in "${RELEASE_CONFIGS[@]}"; do
        local executable="$BUILD_DIR/$current_platform/$config/dht_crawler"
        if [ -f "$executable" ]; then
            local size=$(du -h "$executable" | cut -f1)
            print_success "$config: $executable ($size)"
        else
            print_error "$config: Build failed"
        fi
    done
    
    echo ""
    print_status "Package files created in: $BUILD_DIR/package/"
    print_status "CMake config files: $BUILD_DIR/$current_platform/*/DHTCrawlerConfig.cmake"
}

# Main execution
main() {
    print_status "Starting DHT Crawler Release Build"
    echo "========================================"
    
    # Parse command line arguments
    CLEAN=false
    ALL_PLATFORMS=false
    PACKAGE=false
    TEST=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                CLEAN=true
                shift
                ;;
            --all-platforms)
                ALL_PLATFORMS=true
                shift
                ;;
            --package)
                PACKAGE=true
                shift
                ;;
            --test)
                TEST=true
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo "Options:"
                echo "  --clean           Clean build directory before building"
                echo "  --all-platforms   Build for all platforms (requires toolchains)"
                echo "  --package         Create CMake package after building"
                echo "  --test            Run tests after building"
                echo "  --help            Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Execute build steps
    check_dependencies
    
    if [ "$CLEAN" = true ]; then
        clean_build
    fi
    
    create_build_structure
    
    if [ "$ALL_PLATFORMS" = true ]; then
        build_all_platforms
    else
        build_current_platform
    fi
    
    if [ "$PACKAGE" = true ]; then
        create_package
    fi
    
    if [ "$TEST" = true ]; then
        run_tests
    fi
    
    show_summary
    
    print_success "Build completed successfully!"
}

# Run main function with all arguments
main "$@"
