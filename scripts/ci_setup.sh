#!/bin/bash

# CI/CD Setup Script for DHT Crawler
# This script sets up the CI/CD environment and runs the pipeline

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${BLUE}[HEADER]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check system requirements
check_requirements() {
    print_header "Checking system requirements..."
    
    local missing_deps=()
    
    if ! command_exists cmake; then
        missing_deps+=("cmake")
    fi
    
    if ! command_exists gcc; then
        missing_deps+=("gcc")
    fi
    
    if ! command_exists g++; then
        missing_deps+=("g++")
    fi
    
    if ! command_exists make; then
        missing_deps+=("make")
    fi
    
    if ! command_exists git; then
        missing_deps+=("git")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_error "Please install the missing dependencies and try again."
        exit 1
    fi
    
    print_status "All system requirements satisfied"
}

# Function to install dependencies
install_dependencies() {
    print_header "Installing dependencies..."
    
    if command_exists apt-get; then
        # Ubuntu/Debian
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libboost-all-dev \
            libmysqlclient-dev \
            libssl-dev \
            libcurl4-openssl-dev \
            clang-format \
            clang-tidy \
            cppcheck \
            valgrind \
            gdb \
            docker.io \
            docker-compose
    elif command_exists yum; then
        # CentOS/RHEL
        sudo yum update -y
        sudo yum install -y \
            gcc-c++ \
            cmake \
            boost-devel \
            mysql-devel \
            openssl-devel \
            libcurl-devel \
            clang \
            cppcheck \
            valgrind \
            gdb \
            docker \
            docker-compose
    elif command_exists brew; then
        # macOS
        brew update
        brew install \
            cmake \
            boost \
            mysql \
            openssl \
            curl \
            clang-format \
            clang-tidy \
            cppcheck \
            valgrind \
            gdb \
            docker \
            docker-compose
    else
        print_error "Unsupported package manager. Please install dependencies manually."
        exit 1
    fi
    
    print_status "Dependencies installed successfully"
}

# Function to set up Docker
setup_docker() {
    print_header "Setting up Docker..."
    
    if ! command_exists docker; then
        print_error "Docker is not installed. Please install Docker and try again."
        exit 1
    fi
    
    # Start Docker service
    if command_exists systemctl; then
        sudo systemctl start docker
        sudo systemctl enable docker
    fi
    
    # Add user to docker group
    sudo usermod -aG docker $USER
    
    print_status "Docker setup completed"
}

# Function to build the project
build_project() {
    print_header "Building project..."
    
    # Create build directory
    mkdir -p build
    
    # Configure CMake
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_TESTING=ON \
        -DENABLE_COVERAGE=ON
    
    # Build the project
    cmake --build build --config Release --parallel
    
    print_status "Project built successfully"
}

# Function to run tests
run_tests() {
    print_header "Running tests..."
    
    # Run unit tests
    ctest --test-dir build --output-on-failure
    
    # Run integration tests
    if [ -f "build/integration_tests" ]; then
        ./build/integration_tests
    fi
    
    # Run performance tests
    if [ -f "build/performance_tests" ]; then
        ./build/performance_tests
    fi
    
    print_status "All tests passed"
}

# Function to run code quality checks
run_code_quality() {
    print_header "Running code quality checks..."
    
    # Check code formatting
    find src tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run --Werror
    
    # Run cppcheck
    cppcheck --enable=all --error-exitcode=1 --suppress=missingIncludeSystem src/
    
    # Run clang-tidy
    find src -name "*.cpp" | xargs clang-tidy -p build/
    
    print_status "Code quality checks passed"
}

# Function to run security scan
run_security_scan() {
    print_header "Running security scan..."
    
    # Run Trivy if available
    if command_exists trivy; then
        trivy fs .
    else
        print_warning "Trivy not available. Skipping security scan."
    fi
    
    print_status "Security scan completed"
}

# Function to run performance analysis
run_performance_analysis() {
    print_header "Running performance analysis..."
    
    # Run Valgrind if available
    if command_exists valgrind; then
        valgrind --tool=memcheck --leak-check=full ./build/dht_crawler --help
    else
        print_warning "Valgrind not available. Skipping memory analysis."
    fi
    
    print_status "Performance analysis completed"
}

# Function to run database tests
run_database_tests() {
    print_header "Running database tests..."
    
    # Start MySQL container
    docker-compose -f docker-compose.test.yml up -d mysql
    
    # Wait for MySQL to be ready
    while ! docker-compose -f docker-compose.test.yml exec mysql mysqladmin ping -h localhost -u keynetworks -pK3yn3tw0rk5 --silent; do
        echo "Waiting for MySQL..."
        sleep 2
    done
    
    # Run database tests
    docker-compose -f docker-compose.test.yml run --rm migration_test
    
    # Stop MySQL container
    docker-compose -f docker-compose.test.yml down
    
    print_status "Database tests completed"
}

# Function to generate reports
generate_reports() {
    print_header "Generating reports..."
    
    # Generate coverage report
    if [ -f "build/coverage.info" ]; then
        genhtml build/coverage.info -o build/coverage-report
        print_status "Coverage report generated: build/coverage-report/index.html"
    fi
    
    # Generate performance report
    if [ -f "build/performance-results.json" ]; then
        print_status "Performance report generated: build/performance-results.json"
    fi
    
    print_status "Reports generated successfully"
}

# Function to clean up
cleanup() {
    print_header "Cleaning up..."
    
    # Remove build directory
    rm -rf build
    
    # Stop Docker containers
    docker-compose -f docker-compose.test.yml down -v
    
    print_status "Cleanup completed"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --setup           Set up CI/CD environment"
    echo "  --build           Build the project"
    echo "  --test            Run tests"
    echo "  --quality         Run code quality checks"
    echo "  --security        Run security scan"
    echo "  --performance     Run performance analysis"
    echo "  --database        Run database tests"
    echo "  --all             Run all CI/CD steps"
    echo "  --cleanup         Clean up build artifacts"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --setup"
    echo "  $0 --all"
    echo "  $0 --test --quality"
}

# Main function
main() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    cd "$script_dir/.."
    
    # Check if no arguments provided
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --setup)
                check_requirements
                install_dependencies
                setup_docker
                shift
                ;;
            --build)
                build_project
                shift
                ;;
            --test)
                run_tests
                shift
                ;;
            --quality)
                run_code_quality
                shift
                ;;
            --security)
                run_security_scan
                shift
                ;;
            --performance)
                run_performance_analysis
                shift
                ;;
            --database)
                run_database_tests
                shift
                ;;
            --all)
                check_requirements
                install_dependencies
                setup_docker
                build_project
                run_tests
                run_code_quality
                run_security_scan
                run_performance_analysis
                run_database_tests
                generate_reports
                shift
                ;;
            --cleanup)
                cleanup
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    print_status "CI/CD pipeline completed successfully!"
}

# Run main function with all arguments
main "$@"
