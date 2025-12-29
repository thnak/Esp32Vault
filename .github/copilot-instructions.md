# ESP32 Vault - Copilot Instructions

## Environment
This environment uses espressif/idf:release-v5.4 container. You can use all available command dev tools from it like idf.py set-target, idf.py build.

## Project Overview
ESP32 Vault is a signal telemetry system for ESP32 with ESP-IDF framework, featuring:
- Signal capture and telemetry (raw edge changes and pulse width measurements)
- WiFi management with MQTT-based configuration
- MQTT 5.0 connectivity with binary payloads
- OTA firmware updates
- PSRAM offline buffering (4MB)
- Linux host testing support (experimental)

## Project Structure

```
Esp32Vault/
├── main/                          # Main ESP32 firmware
│   ├── main.cpp                   # Application entry point (app_main)
│   ├── WiFiManager.h/cpp          # WiFi management
│   ├── MQTTManager.h/cpp          # MQTT5 client
│   ├── OTAManager.h/cpp           # OTA updates
│   ├── SignalTelemetry.h/cpp      # Signal capture system
│   └── PSRAMBufferManager.h/cpp   # PSRAM offline buffer
├── test/
│   └── host_test_linux/           # Linux host unit tests
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults     # CONFIG_IDF_TARGET="linux"
│       └── main/
│           ├── test_main.c
│           ├── test_signal_telemetry.c
│           └── test_psram_buffer.c
├── demo/
│   └── linux_demo/                # Linux host demo
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults     # CONFIG_IDF_TARGET="linux"
│       └── main/
│           └── demo_main.c
├── CMakeLists.txt                 # ESP-IDF root build file
├── sdkconfig.defaults             # ESP32 default configuration
├── partitions.csv                 # Partition table
├── build_and_test_linux.sh        # Build and run Linux tests
└── build_and_run_demo_linux.sh    # Build and run Linux demo
```

## Building for ESP32

### Standard Build
```bash
# Set target (default is esp32, but can specify esp32s2, esp32s3, etc.)
idf.py set-target esp32

# Build firmware
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### Key Configuration
- Target: ESP32 with PSRAM (4MB recommended)
- Flash size: 4MB
- PSRAM: Enabled (CONFIG_SPIRAM=y)
- MQTT: MQTT 5.0 protocol (CONFIG_MQTT_PROTOCOL_5=y)
- Partition table: Custom (partitions.csv)

## Linux Host Testing (Experimental)

### Building Linux Tests
```bash
# Linux target requires --preview flag (experimental feature)
cd test/host_test_linux
idf.py --preview set-target linux
idf.py build
./build/esp32vault_host_tests.elf
```

Or use the convenience script:
```bash
./build_and_test_linux.sh
```

### Building Linux Demo
```bash
cd demo/linux_demo
idf.py --preview set-target linux
idf.py build
./build/esp32vault_linux_demo.elf
```

Or use the convenience script:
```bash
./build_and_run_demo_linux.sh
```

## Testing

### Unit Tests (Linux Host)
- Location: `test/host_test_linux/`
- Framework: Unity (included with ESP-IDF)
- Tests:
  - Packet structure sizes and binary layout
  - RawPacket, PulsePacket, DiagPacket serialization
  - Circular buffer operations and overflow handling
- All tests must pass before merging changes

### Running Tests
```bash
./build_and_test_linux.sh
```

Expected output: `6 Tests 0 Failures 0 Ignored`

## Code Style and Conventions

### General
- Use ESP-IDF logging macros (ESP_LOGI, ESP_LOGW, ESP_LOGE)
- Follow ESP-IDF component structure
- Use FreeRTOS tasks and synchronization primitives
- Prefer lock-free ring buffers for ISR-to-task communication

### Binary Packet Structures
- Always use `#pragma pack(push, 1)` for binary packets
- Define structures with explicit field sizes (uint8_t, uint32_t, etc.)
- Test packet sizes in unit tests to ensure correct serialization
- Structures are in `SignalTelemetry.h`

### MQTT Topics
- `raw/{pin}` - Raw edge batches (binary)
- `pulse/{pin}` - Pulse width measurements (binary)
- `diag` - Diagnostic data (binary)
- `heartbeat` - Device heartbeat (JSON)
- `esp32vault/{mac}/status` - Device status (JSON)
- `esp32vault/{mac}/cmd/#` - Command topics

### Memory Management
- Use PSRAM for large buffers (PSRAMBufferManager)
- Avoid malloc in ISR handlers
- Use FreeRTOS queues and ring buffers for inter-task communication
- Monitor free heap in status messages

## Common Tasks

### Adding New Tests
1. Create test file in `test/host_test_linux/main/test_*.c`
2. Add to `main/CMakeLists.txt` SRCS list
3. Declare test function in `test_main.c`
4. Add `RUN_TEST()` call in `app_main()`
5. Build and verify all tests pass

### Modifying Packet Structures
1. Update structure in `SignalTelemetry.h`
2. Update corresponding test in `test_signal_telemetry.c`
3. Update demo in `demo_main.c` if demonstrating the packet
4. Update binary parser examples in documentation
5. Verify packet size tests still pass

### Adding MQTT Commands
1. Add command handler in `handleMQTTMessage()` in `main.cpp`
2. Parse JSON with cJSON
3. Validate all required fields
4. Document in README.md under "MQTT Commands"
5. Add example to `example_mqtt_commands.md`

## Important Files

### Documentation
- `README.md` - Main project documentation
- `LINUX_HOST_TESTING.md` - Comprehensive Linux testing guide
- `SIGNAL_TELEMETRY.md` - Signal capture documentation
- `PSRAM_OFFLINE_BUFFER.md` - PSRAM buffer documentation
- `MQTT5_IMPLEMENTATION.md` - MQTT features guide
- `ESP_IDF_BUILD.md` - Build instructions
- `TESTING.md` - Testing procedures

### Configuration
- `sdkconfig.defaults` - ESP32 default config
- `test/host_test_linux/sdkconfig.defaults` - Linux test config
- `partitions.csv` - Partition layout

### Build Scripts
- `build_and_test_linux.sh` - Build and run unit tests
- `build_and_run_demo_linux.sh` - Build and run demo

## Debugging Tips

### Build Issues
- Ensure ESP-IDF v5.4 or later is used
- For Linux target, always use `--preview` flag
- Check component dependencies in `main/CMakeLists.txt`
- Clean build: `idf.py fullclean`

### Test Failures
- Check packet structure alignment (use `#pragma pack`)
- Verify test expectations match implementation
- Run tests individually for debugging
- Check for memory leaks with valgrind (Linux tests)

### MQTT Issues
- Verify MQTT broker is accessible
- Check WiFi connection status
- Monitor serial output for MQTT events
- Use mosquitto_sub to verify published topics
- Check MQTT 5.0 support on broker

## Dependencies

### ESP-IDF Components
- nvs_flash - Non-volatile storage
- esp_wifi - WiFi management
- esp_netif - Network interface
- mqtt - MQTT 5.0 client
- esp_https_ota - OTA updates
- driver - GPIO, RMT, timers
- esp_timer - High-resolution timing
- json (cJSON) - JSON parsing
- unity - Test framework (for Linux tests)

### External
- None (all dependencies via ESP-IDF)

## Performance Considerations
- ISR latency: < 5 microseconds typical
- Timestamp resolution: 1 microsecond
- Max edges/second: ~10,000 (with batching)
- Batch publish latency: < 50ms typical
- PSRAM buffer: 4MB for offline storage

## Security Notes
- Use TLS for MQTT in production
- Store WiFi credentials in NVS (encrypted option available)
- OTA updates support integrity verification
- Default hotspot credentials should be changed (see WiFiManager.h)
- Binary payloads should be validated server-side

## CI/CD Integration
Tests can run in GitHub Actions or similar:
```yaml
- name: Run Linux Host Tests
  run: |
    docker pull espressif/idf:release-v5.4
    ./build_and_test_linux.sh
```
