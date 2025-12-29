# ESP32 Vault - End-to-End Tests

This directory contains end-to-end integration tests for ESP32 Vault that simulate a complete signal capture, buffering, and MQTT telemetry workflow.

## What Gets Tested

The end-to-end tests verify the complete data flow:

1. **Signal Generation**: Simulates GPIO pin state changes
2. **Signal Capture**: Tests raw edge capture and pulse width measurement
3. **PSRAM Buffering**: 
   - Tests buffer fill under normal conditions
   - Tests buffer overflow and oldest-drop policy
   - Tests buffer replay after connection restoration
4. **MQTT Operations**:
   - Mock MQTT broker for testing publish/subscribe
   - Tests message publishing with binary payloads
   - Tests packet serialization and delivery
   - Tests reconnection and replay scenarios

## Running Tests

### Using Docker (Recommended)

```bash
# From repository root
./build_and_run_e2e_linux.sh
```

### Manual Build

```bash
cd test/e2e_test_linux
idf.py --preview set-target linux
idf.py build
./build/esp32vault_e2e_tests.elf
```

## Test Scenarios

### Scenario 1: Normal Operation
- Signal events generated at moderate rate
- MQTT connection is stable
- Packets published successfully
- PSRAM buffer remains mostly empty

### Scenario 2: PSRAM Buffer Pressure
- High-rate signal events generated
- MQTT connection simulated as slow/disconnected
- PSRAM buffer fills up
- Tests oldest-drop policy when buffer is full
- Tests replay when connection restored

### Scenario 3: MQTT Reconnection
- Start with MQTT disconnected
- Buffer events in PSRAM
- Simulate MQTT connection
- Verify all buffered events replayed in order

### Scenario 4: Mixed Signal Types
- Generate both raw edges and pulse width events
- Verify both packet types published correctly
- Test diagnostic packet generation

## Expected Output

```
===========================================
ESP32 Vault - End-to-End Integration Tests
===========================================

--- E2E Test Scenarios ---
./main/test_main.c:XX:test_e2e_signal_to_mqtt:PASS
./main/test_main.c:XX:test_e2e_psram_buffer_pressure:PASS
./main/test_main.c:XX:test_e2e_mqtt_reconnection:PASS
./main/test_main.c:XX:test_e2e_mixed_signals:PASS

-----------------------
4 Tests 0 Failures 0 Ignored 
OK
```

## Architecture

The E2E tests use:
- **Mock MQTT Broker**: In-memory broker that simulates MQTT publish/subscribe
- **Signal Simulator**: Generates test signal events with controlled timing
- **PSRAM Buffer**: Uses actual PSRAMBufferManager implementation
- **Packet Validators**: Verifies packet structure and content

## Limitations

Like all Linux host tests, hardware-specific features are mocked:
- No real GPIO operations
- No actual network I/O
- PSRAM uses regular malloc (not external PSRAM)
- Timing is simulated, not microsecond-accurate

However, the data flow, buffering logic, and packet serialization are all real implementations.
