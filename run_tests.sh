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

# Run tests and generate JUnit XML report
echo -e "${YELLOW}Running tests...${NC}"
echo "========================================"
./Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests test_results.xml
TEST_RESULT=$?

# Display test summary
if [ $TEST_RESULT -eq 0 ]; then
    echo "========================================"
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo "========================================"
    echo -e "${RED}Tests failed${NC}"
fi

echo -e "${BLUE}Test results saved to:${NC} $(pwd)/test_results.xml"
echo "========================================"

# Return the original test result
exit $TEST_RESULT
