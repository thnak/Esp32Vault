#!/bin/bash
# ESP32 Vault - Build and Run Linux Demo
# This script builds and runs the demo application on Linux host

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DEMO_DIR="$SCRIPT_DIR/demo/linux_demo"

echo "==========================================="
echo "ESP32 Vault - Linux Demo"
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

# Build demo
echo "Building demo..."
if [ "$USE_DOCKER" = true ]; then
    docker run --rm -v "$SCRIPT_DIR":/project -w /project/demo/linux_demo \
        espressif/idf:release-v5.4 \
        bash -c "idf.py --preview set-target linux && idf.py build"
else
    cd "$DEMO_DIR"
    idf.py --preview set-target linux
    idf.py build
fi

echo ""
echo "Build completed successfully!"
echo ""

# Run demo
echo "Running demo..."
if [ "$USE_DOCKER" = true ]; then
    docker run --rm -v "$SCRIPT_DIR":/project -w /project/demo/linux_demo \
        espressif/idf:release-v5.4 \
        ./build/esp32vault_linux_demo.elf
else
    cd "$DEMO_DIR"
    ./build/esp32vault_linux_demo.elf
fi

echo ""
echo "==========================================="
echo "Demo completed!"
echo "==========================================="
