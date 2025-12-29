# Linux Host Testing Guide

ESP32 Vault includes experimental support for Linux host testing using ESP-IDF v5.4's Linux target feature. This allows you to build and run unit tests and demo applications on your development machine without requiring ESP32 hardware.

## Overview

The Linux host testing support includes:

1. **Unit Tests** (`test/host_test_linux/`) - Tests for core data structures and packet serialization
2. **Demo Application** (`demo/linux_demo/`) - Interactive demonstration of signal telemetry packet formats

## Quick Start

### Prerequisites

- Docker (recommended) OR
- ESP-IDF v5.4 or later installed natively

### Running Tests (Using Docker)

```bash
# From repository root
./build_and_test_linux.sh
```

### Running Demo (Using Docker)

```bash
# From repository root
./build_and_run_demo_linux.sh
```

## What Gets Tested

### Unit Tests

The unit tests verify:

- **Packet Structure Sizes**: Ensures binary packet structures have correct sizes for efficient serialization
- **RawPacket**: Tests raw edge change packet creation and field access
- **PulsePacket**: Tests pulse width measurement packet structure
- **DiagPacket**: Tests diagnostic packet structure
- **Buffer Operations**: Tests circular buffer write/read operations and overflow handling

All tests use the Unity testing framework included with ESP-IDF.

### Demo Application

The demo demonstrates:

- Creating and populating raw edge packets simulating a square wave signal
- Creating pulse width packets with timing measurements
- Creating diagnostic packets with system counters
- Binary serialization and hex visualization of all packet types
- Practical examples of packet formats used by ESP32 Vault

## Why Linux Host Testing?

1. **Faster Development**: Build and test without flashing to hardware
2. **CI/CD Integration**: Run tests in continuous integration pipelines
3. **Debugging**: Use native debugging tools (gdb, valgrind, etc.)
4. **Learning**: Understand packet formats without ESP32 hardware
5. **Validation**: Verify protocol implementations before deployment

## Technical Details

### ESP-IDF Linux Target

ESP-IDF v5.4 includes experimental Linux target support that allows building firmware for Linux. This uses:

- Standard Linux libraries instead of ESP32-specific hardware
- FreeRTOS simulator for Linux
- Mock implementations for hardware-dependent features

### Limitations

The Linux target has some limitations:

- No actual GPIO or hardware peripherals
- No WiFi or Bluetooth connectivity
- No PSRAM (uses regular malloc)
- Some ESP32-specific APIs are mocked or unavailable
- Marked as experimental/preview feature (requires `--preview` flag)

### What Works on Linux

- Memory management (heap, malloc)
- FreeRTOS tasks and synchronization
- Binary packet serialization
- Data structure validation
- Circular buffers and queues
- Unity test framework
- Logging and console I/O

## Directory Structure

```
Esp32Vault/
├── test/
│   └── host_test_linux/          # Unit tests for Linux
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       ├── README.md
│       └── main/
│           ├── CMakeLists.txt
│           ├── test_main.c       # Test runner
│           ├── test_signal_telemetry.c
│           └── test_psram_buffer.c
├── demo/
│   └── linux_demo/               # Demo application
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       ├── README.md
│       └── main/
│           ├── CMakeLists.txt
│           └── demo_main.c
├── build_and_test_linux.sh       # Test build script
└── build_and_run_demo_linux.sh   # Demo build script
```

## Build Process

### Using Build Scripts (Recommended)

The provided scripts handle Docker container setup automatically:

```bash
./build_and_test_linux.sh    # Builds and runs tests
./build_and_run_demo_linux.sh # Builds and runs demo
```

### Manual Build (Docker)

```bash
# Tests
cd test/host_test_linux
docker run --rm -v $(pwd)/../..:/project -w /project/test/host_test_linux \
  espressif/idf:release-v5.4 \
  bash -c "idf.py --preview set-target linux && idf.py build"
docker run --rm -v $(pwd)/../..:/project -w /project/test/host_test_linux \
  espressif/idf:release-v5.4 \
  ./build/esp32vault_host_tests.elf

# Demo
cd demo/linux_demo
docker run --rm -v $(pwd)/../..:/project -w /project/demo/linux_demo \
  espressif/idf:release-v5.4 \
  bash -c "idf.py --preview set-target linux && idf.py build"
docker run --rm -v $(pwd)/../..:/project -w /project/demo/linux_demo \
  espressif/idf:release-v5.4 \
  ./build/esp32vault_linux_demo.elf
```

### Manual Build (Native ESP-IDF)

```bash
# Set up environment
. $IDF_PATH/export.sh

# Tests
cd test/host_test_linux
idf.py --preview set-target linux
idf.py build
./build/esp32vault_host_tests.elf

# Demo
cd demo/linux_demo
idf.py --preview set-target linux
idf.py build
./build/esp32vault_linux_demo.elf
```

## Expected Output

### Unit Tests

```
===========================================
ESP32 Vault - Linux Host Unit Tests
===========================================

--- Signal Telemetry Tests ---
./main/test_main.c:30:test_signal_telemetry_packet_serialization:PASS
./main/test_main.c:31:test_raw_packet_structure:PASS
./main/test_main.c:32:test_pulse_packet_structure:PASS
./main/test_main.c:33:test_diag_packet_structure:PASS

--- PSRAM Buffer Tests ---
./main/test_main.c:37:test_psram_buffer_basic:PASS
./main/test_main.c:38:test_psram_buffer_overflow:PASS

-----------------------
6 Tests 0 Failures 0 Ignored 
OK
```

### Demo Application

```
============================================
ESP32 Vault - Linux Demo
Signal Telemetry Packet Demonstration
============================================

=== Raw Edge Packet Demo ===
Packet Info:
  Version: 1
  Type: 1 (raw)
  Base Time: 795525927 us
  Base Seq: 1000
  Edge Count: 5
  
Edges:
  [0] Pin=14 Value=1 Time=795525927 us (dt=0 us)
  [1] Pin=14 Value=0 Time=795526927 us (dt=1000 us)
  ...

Binary Packet Size: 45 bytes
Binary Payload (hex):
01 01 27 c3 6a 2f 00 00 00 00 e8 03 00 00 05 0e
...
```

## Adding New Tests

To add new tests:

1. Create a new test file in `test/host_test_linux/main/`
2. Add the file to `main/CMakeLists.txt` SRCS list
3. Declare test functions in `test_main.c`
4. Add `RUN_TEST()` calls in `app_main()`
5. Rebuild and run

Example test function:

```c
void test_my_feature(void)
{
    // Arrange
    int expected = 42;
    
    // Act
    int actual = my_function();
    
    // Assert
    TEST_ASSERT_EQUAL_INT(expected, actual);
}
```

## Troubleshooting

### Docker Image Not Found

If you get an error about the Docker image not found, pull it manually:

```bash
docker pull espressif/idf:release-v5.4
```

### Build Errors

1. Ensure you're using ESP-IDF v5.4 or later
2. The `--preview` flag is required for Linux target
3. Check that Docker has sufficient resources allocated

### Permission Errors

If you get permission errors with Docker:

```bash
# Add your user to docker group (Linux)
sudo usermod -aG docker $USER
# Then log out and back in
```

## CI/CD Integration

You can integrate these tests into your CI/CD pipeline:

```yaml
# GitHub Actions example
- name: Run Linux Host Tests
  run: |
    docker pull espressif/idf:release-v5.4
    ./build_and_test_linux.sh
```

## Future Enhancements

Potential improvements:

- Add tests for WiFiManager logic
- Add tests for MQTTManager message handling
- Add tests for OTA update validation
- Increase test coverage
- Add performance benchmarks
- Integration with code coverage tools

## References

- [ESP-IDF Linux Target Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/linux-host-testing.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [ESP32 Vault Signal Telemetry Documentation](SIGNAL_TELEMETRY.md)
