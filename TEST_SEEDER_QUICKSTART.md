# Test Seeder Quick Start

This guide shows how to quickly build and test the firmware with the test seeder enabled.

## Quick Build and Flash

```bash
# Build with test seeder enabled
idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build

# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py monitor
```

## Expected Output

When the test seeder is running, you should see:

```
I (1234) main: Initializing Test Seeder...
===========================================
TEST SEEDER ENABLED
Generating synthetic telemetry every 1 second
WARNING: This is a test build!
===========================================
I (1240) TestSeeder: Test seeder task started
I (2240) TestSeeder: Seeding test data - iteration 0
I (3240) TestSeeder: Seeding test data - iteration 1
I (4240) TestSeeder: Seeding test data - iteration 2
```

## Monitor MQTT Data

In another terminal, subscribe to the test topics:

```bash
# Monitor all telemetry
mosquitto_sub -h YOUR_BROKER -t "esp32vault/#" -v

# Or monitor specific topics
mosquitto_sub -h YOUR_BROKER -t "esp32vault/raw/14" -F "%t: %l bytes"
mosquitto_sub -h YOUR_BROKER -t "esp32vault/pulse/15" -F "%t: %l bytes"
mosquitto_sub -h YOUR_BROKER -t "esp32vault/diag" -F "%t: %l bytes"
```

## Verify Data with Python

Use the binary parser to decode and verify the packets:

```bash
python binary_parser_example.py --broker YOUR_BROKER --mac YOUR_MAC
```

You should see:
```
Connected to MQTT broker
Subscribed to esp32vault/#

[RAW] Pin 14: 5 edges (seq=0)
  [0] 0 @ 50us
  [1] 1 @ 100us
  [2] 0 @ 150us
  ...

[PULSE] Pin 15: high=234us, low=123us (seq=1)

[DIAG] Raw:0, Pulse:0, Queue:3, RMT:0
```

## Disable Test Seeder

To build production firmware without test seeder:

```bash
# Default build (test seeder disabled)
idf.py build

# Or explicitly disable
idf.py -D CONFIG_ENABLE_TEST_SEEDER=n build
```

## Configuration via menuconfig

For persistent configuration:

```bash
idf.py menuconfig
```

Navigate to: **ESP32 Vault Configuration → Enable Test Seeder**

Check or uncheck the option, save, and rebuild.

## Tips

1. **Stability Testing**: Let it run for hours/days to check for memory leaks or crashes
2. **Network Testing**: Monitor packet loss and latency under continuous load
3. **Server Testing**: Use synthetic data to test your server parsing logic
4. **CI/CD Integration**: Automated builds can use test seeder for validation

## Troubleshooting

### No test data appearing

Check:
- MQTT broker is configured: `mosquitto_pub -h YOUR_BROKER -t "esp32vault/MAC/cmd/mqtt" ...`
- WiFi is connected: Check serial output for WiFi connection logs
- Test seeder is enabled: Look for "TEST SEEDER ENABLED" in serial output

### Build fails

```bash
# Clean and rebuild
idf.py fullclean
idf.py -D CONFIG_ENABLE_TEST_SEEDER=y build
```

### Want to change test parameters

Edit `main/TestSeeder.cpp`:
- Line 64: Change delay (default 1000ms)
- Line 90: Change test pin for raw data (default 14)
- Line 127: Change test pin for pulse data (default 15)
- Lines 93-94: Adjust number of edges (5-10)
- Lines 131-132: Adjust pulse timings

Then rebuild and flash.
