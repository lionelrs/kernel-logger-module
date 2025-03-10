#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color
YELLOW='\033[1;33m'

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
print_header() {
    echo -e "\n${YELLOW}=== $1 ===${NC}"
}

assert() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ $2${NC}"
        echo -e "${RED}  Expected: $3${NC}"
        echo -e "${RED}  Got: $4${NC}"
        ((TESTS_FAILED++))
    fi
}

cleanup() {
    print_header "Cleaning up"
    cd ..
    make unload 2>/dev/null
    make clean 2>/dev/null
}


# Start testing
print_header "Building module"
make clean >/dev/null
make >/dev/null
assert $? "Module compilation" "successful build" "build failed"
print_header "Loading module"
make load >/dev/null
assert $? "Module loading" "successful load" "load failed"

# Test if device file exists
print_header "Device file checks"
if [ -c "/dev/klogger" ]; then
    assert 0 "Device file exists" "file exists" "file not found"
else
    assert 1 "Device file exists" "file exists" "file not found"
fi

# Test permissions
PERMS=$(stat -c "%a" /dev/klogger)
[ "$PERMS" = "666" ]
assert $? "Device file permissions" "666" "$PERMS"

# Test basic write/read operations
print_header "Basic I/O operations"

# Single message test
echo "test_message" > /dev/klogger
READ_RESULT=$(cat /dev/klogger)
EXPECTED="test_message"
[ "$READ_RESULT" = "$EXPECTED" ]
assert $? "Single message read/write" "$EXPECTED" "$READ_RESULT"

# Multiple message test
print_header "Multiple message test"
make reload > /dev/null
for i in {1..5}; do
    echo "msg$i" > /dev/klogger
done
READ_RESULT=$(cat /dev/klogger)
EXPECTED=$'msg1\nmsg2\nmsg3\nmsg4\nmsg5'
[ "$READ_RESULT" = "$EXPECTED" ]
assert $? "Multiple message read" "$EXPECTED" "$READ_RESULT"

# Buffer overflow test
print_header "Buffer overflow test"
make reload > /dev/null
for i in {1..1028}; do
    echo "overflow$i" > /dev/klogger
done
READ_RESULT=$(cat /dev/klogger | tail -4)
EXPECTED=$'overflow1025\noverflow1026\noverflow1027\noverflow1028'
[ "$READ_RESULT" = "$EXPECTED" ]
assert $? "Buffer overflow handling" "$EXPECTED" "$READ_RESULT"

# Concurrent access test
print_header "Concurrent access test"
make reload > /dev/null

(
    for i in {1..5}; do
        echo "concurrent$i" > /dev/klogger &
    done
    wait
)
sleep 1  # Give the kernel time to process
READ_RESULT=$(cat /dev/klogger)
CONCURRENT_COUNT=$(echo "$READ_RESULT" | grep -c "concurrent")
[ "$CONCURRENT_COUNT" -gt 0 ]
assert $? "Concurrent write handling" "concurrent messages present" "$CONCURRENT_COUNT messages found"

# Test module unloading
print_header "Unloading module"
make unload >/dev/null
assert $? "Module unloading" "successful unload" "unload failed"

# Print test summary
print_header "Test Summary"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"

# Cleanup
cleanup

# Exit with failure if any tests failed
[ "$TESTS_FAILED" -eq 0 ] || exit 1