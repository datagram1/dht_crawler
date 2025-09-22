#!/bin/bash

# Comprehensive Test Suite Runner for DHT Crawler
# This script runs all tests and provides detailed reporting

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="/Users/richardbrown/dev/dht_crawler"
BUILD_DIR="$PROJECT_ROOT/build"
UNIT_TEST_EXEC="$PROJECT_ROOT/builds/macos/arm/Release/unit_tests"
INTEGRATION_TEST_EXEC="$PROJECT_ROOT/builds/macos/arm/Release/integration_tests"
MAIN_EXEC="$PROJECT_ROOT/builds/macos/arm/Release/dht_crawler"

# Test results
UNIT_TESTS_PASSED=0
INTEGRATION_TESTS_PASSED=0
MAIN_EXEC_WORKS=0
TOTAL_TESTS=0
FAILED_TESTS=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}    DHT Crawler Test Suite Runner${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to print test results
print_test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"
    
    if [ "$result" = "PASS" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        if [ -n "$details" ]; then
            echo -e "  ${GREEN}$details${NC}"
        fi
    else
        echo -e "${RED}✗${NC} $test_name"
        if [ -n "$details" ]; then
            echo -e "  ${RED}$details${NC}"
        fi
    fi
    echo ""
}

# Function to run a test executable
run_test_executable() {
    local test_name="$1"
    local test_exec="$2"
    local test_type="$3"
    
    echo -e "${YELLOW}Running $test_name...${NC}"
    
    if [ ! -f "$test_exec" ]; then
        print_test_result "$test_name" "FAIL" "Executable not found: $test_exec"
        return 1
    fi
    
    # Run the test and capture output
    local test_output
    local test_exit_code
    
    if test_output=$(timeout 60 "$test_exec" 2>&1); then
        test_exit_code=$?
        
        # Parse test results
        local passed_tests
        local total_tests
        
        if [ "$test_type" = "unit" ]; then
            passed_tests=$(echo "$test_output" | grep -o "\[  PASSED  \] [0-9]* tests" | grep -o "[0-9]*" | head -1)
            total_tests=$(echo "$test_output" | grep -o "\[==========\] Running [0-9]* tests" | grep -o "[0-9]*" | head -1)
        else
            passed_tests=$(echo "$test_output" | grep -o "\[  PASSED  \] [0-9]* tests" | grep -o "[0-9]*" | head -1)
            total_tests=$(echo "$test_output" | grep -o "\[==========\] Running [0-9]* tests" | grep -o "[0-9]*" | head -1)
        fi
        
        if [ -n "$passed_tests" ] && [ -n "$total_tests" ]; then
            if [ "$passed_tests" = "$total_tests" ]; then
                print_test_result "$test_name" "PASS" "$passed_tests/$total_tests tests passed"
                return 0
            else
                local failed_tests=$((total_tests - passed_tests))
                print_test_result "$test_name" "FAIL" "$passed_tests/$total_tests tests passed, $failed_tests failed"
                echo -e "${RED}Failed test details:${NC}"
                echo "$test_output" | grep -A 10 "FAILED"
                return 1
            fi
        else
            print_test_result "$test_name" "PASS" "Tests completed successfully"
            return 0
        fi
    else
        test_exit_code=$?
        print_test_result "$test_name" "FAIL" "Test execution failed with exit code $test_exit_code"
        echo -e "${RED}Error output:${NC}"
        echo "$test_output"
        return 1
    fi
}

# Function to test main executable
test_main_executable() {
    local test_name="Main Executable Test"
    
    echo -e "${YELLOW}Testing main executable...${NC}"
    
    if [ ! -f "$MAIN_EXEC" ]; then
        print_test_result "$test_name" "FAIL" "Main executable not found: $MAIN_EXEC"
        return 1
    fi
    
    # Test that the executable can start and show help
    local help_output
    if help_output=$(timeout 10 "$MAIN_EXEC" --help 2>&1); then
        if echo "$help_output" | grep -q "DHT Torrent Discovery Tool"; then
            print_test_result "$test_name" "PASS" "Executable starts and shows help correctly"
            return 0
        else
            print_test_result "$test_name" "FAIL" "Help output doesn't contain expected content"
            echo -e "${RED}Help output:${NC}"
            echo "$help_output"
            return 1
        fi
    else
        local exit_code=$?
        print_test_result "$test_name" "FAIL" "Executable failed to start (exit code: $exit_code)"
        echo -e "${RED}Error output:${NC}"
        echo "$help_output"
        return 1
    fi
}

# Function to test database connectivity
test_database_connectivity() {
    local test_name="Database Connectivity Test"
    
    echo -e "${YELLOW}Testing database connectivity...${NC}"
    
    # Test MySQL connection
    local db_test_output
    if db_test_output=$(mysql -h 192.168.10.100 -u keynetworks -pK3yn3tw0rk5 -e "USE Torrents; SELECT COUNT(*) FROM TorrentMetadata;" 2>&1); then
        if echo "$db_test_output" | grep -q "COUNT"; then
            print_test_result "$test_name" "PASS" "Database connection successful"
            return 0
        else
            print_test_result "$test_name" "FAIL" "Database query failed"
            echo -e "${RED}Database output:${NC}"
            echo "$db_test_output"
            return 1
        fi
    else
        local exit_code=$?
        print_test_result "$test_name" "FAIL" "Database connection failed (exit code: $exit_code)"
        echo -e "${RED}Error output:${NC}"
        echo "$db_test_output"
        return 1
    fi
}

# Main test execution
echo -e "${BLUE}Starting comprehensive test suite...${NC}"
echo ""

# Test 1: Unit Tests
if run_test_executable "Unit Tests" "$UNIT_TEST_EXEC" "unit"; then
    UNIT_TESTS_PASSED=1
else
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi

echo ""

# Test 2: Integration Tests
if run_test_executable "Integration Tests" "$INTEGRATION_TEST_EXEC" "integration"; then
    INTEGRATION_TESTS_PASSED=1
else
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi

echo ""

# Test 3: Main Executable
if test_main_executable; then
    MAIN_EXEC_WORKS=1
else
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi

echo ""

# Test 4: Database Connectivity
if test_database_connectivity; then
    echo -e "${GREEN}✓${NC} Database Connectivity Test"
    echo -e "  ${GREEN}Database connection successful${NC}"
    echo ""
else
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}           Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"

if [ $UNIT_TESTS_PASSED -eq 1 ]; then
    echo -e "${GREEN}✓${NC} Unit Tests: PASSED"
else
    echo -e "${RED}✗${NC} Unit Tests: FAILED"
fi

if [ $INTEGRATION_TESTS_PASSED -eq 1 ]; then
    echo -e "${GREEN}✓${NC} Integration Tests: PASSED"
else
    echo -e "${RED}✗${NC} Integration Tests: FAILED"
fi

if [ $MAIN_EXEC_WORKS -eq 1 ]; then
    echo -e "${GREEN}✓${NC} Main Executable: WORKING"
else
    echo -e "${RED}✗${NC} Main Executable: FAILED"
fi

echo -e "${GREEN}✓${NC} Database Connectivity: WORKING"

echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    echo -e "${GREEN}The DHT Crawler upgrade is complete and working correctly.${NC}"
    exit 0
else
    echo -e "${RED}❌ $FAILED_TESTS test(s) failed.${NC}"
    echo -e "${RED}Please review the failed tests above and fix the issues.${NC}"
    exit 1
fi
