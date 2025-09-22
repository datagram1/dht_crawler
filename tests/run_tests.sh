#!/bin/bash

# Test runner script for DHT Crawler
# This script builds and runs all tests

set -e

echo "=== DHT Crawler Test Runner ==="
echo "Building and running tests..."

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Create build directory for tests
BUILD_DIR="$PROJECT_ROOT/build/test"
mkdir -p "$BUILD_DIR"

# Change to build directory
cd "$BUILD_DIR"

echo "Building tests..."
cmake "$PROJECT_ROOT" -DENABLE_TESTING=ON
make -j$(nproc)

echo "Running unit tests..."
./unit_tests

echo "Running integration tests..."
./integration_tests

echo "Running all tests with CTest..."
ctest --verbose

echo "=== All tests completed successfully! ==="
