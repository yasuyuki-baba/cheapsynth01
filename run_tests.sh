#!/bin/bash

# Script to build and run tests

# Color definitions
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${YELLOW}CheapSynth01 Test Runner${NC}"
echo "========================================"

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build_tests
cd build_tests

# Run CMake
echo -e "${YELLOW}Running CMake...${NC}"
cmake .. -DSTANDALONE_ONLY=ON
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
echo -e "${YELLOW}Building tests...${NC}"
make CheapSynth01Tests
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Run tests
echo -e "${YELLOW}Running tests...${NC}"
echo "========================================"
./Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests
TEST_RESULT=$?

# Check if test results XML exists
if [ -f "test_results.xml" ]; then
    echo -e "${BLUE}Test results saved to:${NC} $(pwd)/test_results.xml"
    
    # Count passed and failed tests
    TOTAL_TESTS=$(grep -c "<testcase" test_results.xml)
    FAILED_TESTS=$(grep -c "<failure" test_results.xml)
    PASSED_TESTS=$((TOTAL_TESTS - FAILED_TESTS))
    
    echo "========================================"
    echo -e "${BLUE}Test Summary:${NC}"
    echo -e "Total: ${TOTAL_TESTS}, Passed: ${GREEN}${PASSED_TESTS}${NC}, Failed: ${RED}${FAILED_TESTS}${NC}"
    
    # If there are failures, print them
    if [ $FAILED_TESTS -gt 0 ]; then
        echo "========================================"
        echo -e "${YELLOW}Failed Tests:${NC}"
        grep -B 1 -A 1 "<failure" test_results.xml | grep -v "</failure>" | sed 's/<failure message="/ - /g' | sed 's/".*>//g' | sed 's/<testcase name="/ /g' | sed 's/" classname=.*//g'
    fi
else
    echo -e "${YELLOW}No test results file found${NC}"
fi

# Check test results
if [ $TEST_RESULT -eq 0 ]; then
    echo "========================================"
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo "========================================"
    echo -e "${RED}Tests failed${NC}"
    exit 1
fi
