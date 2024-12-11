#!/bin/bash -eu

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check if test file argument is provided
if [ $# -lt 1 ]; then
    echo -e "${RED}Usage: $0 <test_file_without_extension>${NC}"
    exit 1
fi

TEST_FILE="../tests/$1.c"

echo "=== Building LLVM Pass ==="
# Compile the test program to LLVM IR
clang  -g -O0 -emit-llvm -c "$TEST_FILE" -o $1.bc -DLLVM_USE_LINKER=lld

echo "=== Running Pass on Test Program ==="
# Run the pass
opt -passes=seminal-input-detector -disable-output $1.bc

echo "=== Checking Logs ==="

LOG_FILE="seminal-values.json"

# Check if log file exists
if [ ! -f "$LOG_FILE" ]; then
    echo -e "${RED}✗ Log file $LOG_FILE not found${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Log file $LOG_FILE found${NC}"
fi

# Detailed output
echo -e "\n=== Detailed Output Analysis ==="
echo "JSON Output:"
cat "$LOG_FILE"

# only clean if code is not present
if [ $# -ne 7 ]; then
    # Cleanup
    rm -f test.ll instrumented.ll test_program "$LOG_FILE" $1.bc
fi

echo -e "\n=== Test Complete ==="