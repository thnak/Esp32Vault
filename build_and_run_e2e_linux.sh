#!/bin/bash
# ESP32 Vault - Build and Run Linux E2E Tests
# This script builds and runs end-to-end integration tests on Linux host

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$SCRIPT_DIR/test/e2e_test_linux"

echo "==========================================="
echo "ESP32 Vault - E2E Integration Tests"
echo "==========================================="
echo ""

# Check if running in Docker or native
if [ -f /.dockerenv ]; then
    echo "Running in Docker container"
    USE_DOCKER=false
else
    echo "Running on host system"
    USE_DOCKER=true
fi

# Build tests
echo "Building E2E tests..."
if [ "$USE_DOCKER" = true ]; then
    docker run --rm -v "$SCRIPT_DIR":/project -w /project/test/e2e_test_linux \
        espressif/idf:release-v5.4 \
        bash -c "idf.py --preview set-target linux && idf.py build"
else
    cd "$TEST_DIR"
    idf.py --preview set-target linux
    idf.py build
fi

echo ""
echo "Build completed successfully!"
echo ""

# Run tests
echo "Running E2E tests..."
if [ "$USE_DOCKER" = true ]; then
    docker run --rm -v "$SCRIPT_DIR":/project -w /project/test/e2e_test_linux \
        espressif/idf:release-v5.4 \
        ./build/esp32vault_e2e_tests.elf
else
    cd "$TEST_DIR"
    ./build/esp32vault_e2e_tests.elf
fi

echo ""
echo "==========================================="
echo "E2E tests completed!"
echo "==========================================="
