# Test Seeder - Firmware Stability Testing

## Overview

The Test Seeder is a build-time optional component that generates synthetic telemetry data for firmware stability testing. When enabled, it publishes all types of telemetry packets (raw edges, pulse widths, and diagnostics) every second without requiring actual GPIO signal inputs.

## Purpose

The Test Seeder allows validation of:

- **MQTT Connectivity**: Verify broker connection stability under continuous load
- **Binary Serialization**: Ensure packet structures are correctly formatted
- **Network Throughput**: Monitor data transmission rates and reliability
- **PSRAM Buffer**: Test offline buffering when MQTT is unavailable
- **Firmware Stability**: Long-running stress test for memory leaks or crashes
- **Server Integration**: Validate server-side packet parsing and processing

## Build Configuration

### Enabling Test Seeder

The test seeder is controlled by the `CONFIG_ENABLE_TEST_SEEDER` build flag.

#### Method 1: Using menuconfig

```bash
idf.py menuconfig
```

Navigate to: `ESP32 Vault Configuration > Enable Test Seeder`

Check the option and save.

#### Method 2: Using sdkconfig

Add to your `sdkconfig` or `sdkconfig.defaults`:

```
CONFIG_ENABLE_TEST_SEEDER=y
```

#### Method 3: Command line

```bash
idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build
```

### Building with Test Seeder

```bash
# Enable test seeder
idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### Disabling Test Seeder

For production builds, ensure the flag is disabled (default):

```bash
idf.py build  # Test seeder is disabled by default
```

## Generated Telemetry

When enabled, the test seeder publishes the following every **1 second**:

### 1. Raw Edge Packets

- **Topic**: `esp32vault/raw/14`
- **Pin**: GPIO 14 (simulated)
- **Content**: 5-10 synthetic edge changes per packet
- **Timing**: Variable delays (50-200µs between edges)
- **Values**: Alternating HIGH/LOW states

### 2. Pulse Width Packets

- **Topic**: `esp32vault/pulse/15`
- **Pin**: GPIO 15 (simulated)
- **High Time**: 100-500µs (varies per iteration)
- **Low Time**: 50-300µs (varies per iteration)

### 3. Diagnostic Packets

- **Topic**: `esp32vault/diag`
- **Content**: System health metrics
  - Dropped packets: 0 (simulated)
  - Queue depth: 0-9 (cycles through values)
  - RMT overflow: 0 (simulated)

## Monitoring Test Seeder

### Serial Output

When test seeder is enabled, you'll see:

```
===========================================
TEST SEEDER ENABLED
Generating synthetic telemetry every 1 second
WARNING: This is a test build!
===========================================

I (5234) TestSeeder: Test seeder task started
I (6234) TestSeeder: Seeding test data - iteration 0
I (7234) TestSeeder: Seeding test data - iteration 1
...
```

### MQTT Monitoring

Subscribe to test topics:

```bash
# Monitor raw packets
mosquitto_sub -h broker.example.com -t "esp32vault/raw/14" -F "%t: %l bytes"

# Monitor pulse packets
mosquitto_sub -h broker.example.com -t "esp32vault/pulse/15" -F "%t: %l bytes"

# Monitor diagnostics
mosquitto_sub -h broker.example.com -t "esp32vault/diag" -F "%t: %l bytes"

# Monitor all telemetry
mosquitto_sub -h broker.example.com -t "esp32vault/#" -v
```

## Use Cases

### 1. CI/CD Integration

Add test seeder builds to your continuous integration pipeline:

```yaml
# .github/workflows/firmware-test.yml
- name: Build Test Firmware
  run: |
    idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build
    
- name: Flash and Monitor
  run: |
    idf.py -p /dev/ttyUSB0 flash
    timeout 300 idf.py monitor  # Run for 5 minutes
```

### 2. Long-Running Stability Test

```bash
# Build and flash test firmware
idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build flash

# Monitor for 24 hours
idf.py monitor | tee stability-test.log

# Analyze results
grep "ERROR\|WARN\|Heap" stability-test.log
```

### 3. Network Performance Testing

Monitor packet delivery rates and latency with the test seeder running continuously.

### 4. Server-Side Validation

Use test seeder data to validate your server's packet parsing and storage logic without requiring actual hardware signals.

## Performance Impact

The test seeder has minimal performance impact:

- **Task Priority**: Medium (5)
- **Stack Size**: 4KB
- **CPU Usage**: < 1% (publishes every 1 second)
- **Memory**: ~4KB for task stack + packet buffers
- **Network**: ~500 bytes/second (3 packets per second)

## Data Format

All packets follow the standard ESP32 Vault binary format:

### Raw Packet Structure
```
PacketHeader (2 bytes)
baseTimeUs (8 bytes)
baseSeq (4 bytes)
count (1 byte)
RawEdge[] (6 bytes each)
```

### Pulse Packet Structure
```
PacketHeader (2 bytes)
pinId (1 byte)
highUs (4 bytes)
lowUs (4 bytes)
deviceTimeUs (8 bytes)
seq (4 bytes)
```

### Diagnostic Packet Structure
```
PacketHeader (2 bytes)
droppedRaw (4 bytes)
droppedPulse (4 bytes)
queueDepth (2 bytes)
rmtOverflow (2 bytes)
```

## Warnings

⚠️ **IMPORTANT**: The test seeder should ONLY be enabled for testing purposes.

- **Do NOT** enable in production firmware
- **Do NOT** use with real signal capture (will generate redundant data)
- **Do NOT** rely on test data for actual signal analysis
- Test seeder always uses pins 14 and 15 for simulated data

## Troubleshooting

### Test Seeder Not Starting

Check serial output for:
```
I (xxxx) TestSeeder: Test seeder task started
```

If missing:
- Verify `CONFIG_ENABLE_TEST_SEEDER=y` in sdkconfig
- Check WiFi and MQTT are connected
- Monitor heap: `esp_get_free_heap_size()`

### No MQTT Data

```
D (xxxx) TestSeeder: MQTT not connected, skipping iteration X
```

Solution: Ensure MQTT broker is configured and device is connected to WiFi.

### Build Errors

If you see compilation errors related to TestSeeder:
- Clean build: `idf.py fullclean`
- Rebuild: `idf.py build`
- Verify all files are present: `TestSeeder.h`, `TestSeeder.cpp`, `Kconfig.projbuild`

## Related Documentation

- [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md) - Signal capture system
- [MQTT5_IMPLEMENTATION.md](MQTT5_IMPLEMENTATION.md) - MQTT protocol details
- [TESTING.md](TESTING.md) - Testing procedures
- [binary_parser_example.py](binary_parser_example.py) - Python parser for test data
