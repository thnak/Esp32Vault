# ESP32 Vault - Linux Host Tests

This directory contains unit tests that can be built and run on a Linux host using ESP-IDF's Linux target support.

## Prerequisites

- ESP-IDF v5.4 or later with Linux target support
- Docker (recommended) or native ESP-IDF installation

## Building and Running Tests

### Using Docker (Recommended)

```bash
# Navigate to test directory
cd test/host_test_linux

# Build tests using ESP-IDF v5.4 container
docker run --rm -v $PWD/../..:/project -w /project/test/host_test_linux espressif/idf:release-v5.4 \
  idf.py set-target linux build

# Run tests
docker run --rm -v $PWD/../..:/project -w /project/test/host_test_linux espressif/idf:release-v5.4 \
  ./build/esp32vault_host_tests.elf
```

### Using Native ESP-IDF

```bash
# Navigate to test directory
cd test/host_test_linux

# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Set target to Linux (requires --preview flag as Linux target is experimental)
idf.py --preview set-target linux

# Build tests
idf.py build

# Run tests
./build/esp32vault_host_tests.elf
```

## Test Coverage

The host tests cover:

1. **Signal Telemetry Tests**
   - Binary packet structure sizes
   - RawPacket serialization
   - PulsePacket serialization
   - DiagPacket serialization

2. **PSRAM Buffer Tests**
   - Basic buffer operations (write/read)
   - Buffer overflow handling
   - Circular buffer behavior

## Adding New Tests

1. Create a new test file in `main/` directory (e.g., `test_new_feature.c`)
2. Add the file to `main/CMakeLists.txt` in the `SRCS` list
3. Declare test functions in `test_main.c`
4. Add `RUN_TEST()` calls in `app_main()`

## Notes

- These tests run on Linux and do not require ESP32 hardware
- Hardware-dependent features use mocks or stubs
- Tests use the Unity test framework included with ESP-IDF
