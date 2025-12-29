# Testing Guide for ESP32 Vault

This document provides comprehensive testing procedures for ESP32 Vault Signal Telemetry v1.

## Prerequisites

- ESP32 development board connected to computer
- ESP-IDF v5.x installed and configured
- MQTT broker accessible (e.g., Mosquitto)
- WiFi network (2.4 GHz)
- MQTT client tools (mosquitto_pub, mosquitto_sub, or MQTT Explorer)
- Python 3.x (for binary parser testing)

## 1. Build and Flash Test

### Test: Successful Build

```bash
cd Esp32Vault
idf.py build
```

**Expected Result**: 
- Build completes without errors
- Output shows: `Project build complete`
- Firmware binary at `build/esp32vault.bin`
- Firmware size displayed (should be ~1.5MB)

### Test: Flash Firmware

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Expected Result**:
- Flash completes successfully
- Device resets automatically
- Serial monitor shows startup messages with Signal Telemetry v1

## 2. WiFi Manager Tests

### Test 2.1: First Boot (No Credentials)

**Steps**:
1. Flash firmware to fresh ESP32 (or after erasing NVS: `idf.py erase-flash`)
2. Monitor serial output

**Expected Result**:
```
ESP32 Vault - Signal Telemetry v1
=================================
Initializing WiFi...
No saved credentials. Attempting to connect to default hotspot...
...
Connected to default hotspot!
IP address: 192.168.x.x
Use MQTT to configure new WiFi credentials.
Initializing MQTT...
```

### Test 2.2: WiFi Configuration via MQTT

**Prerequisites**:
1. Set up laptop hotspot with SSID `EspSetup` and password `HeLooWod`
2. Start MQTT broker on laptop (e.g., `mosquitto -v`)

**Steps**:
1. Power on ESP32 (it will connect to the `EspSetup` hotspot)
2. Configure MQTT broker:
   ```bash
   mosquitto_pub -h localhost \
     -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
     -m '{"server": "192.168.x.x", "port": 1883}'
   ```
3. Configure WiFi credentials:
   ```bash
   mosquitto_pub -h localhost \
     -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/wifi" \
     -m '{"ssid": "TestWiFi", "password": "TestPassword"}'
   ```

**Expected Result**:
- Device restarts automatically
- Device connects to WiFi network
- Serial monitor shows:
  ```
  WiFi connected!
  IP address: 192.168.1.XXX
  ```

### Test 2.3: WiFi Reconnection After Restart

**Steps**:
1. Configure WiFi (Test 2.2)
2. Reset ESP32 (press reset button)
3. Monitor serial output

**Expected Result**:
```
Attempting to connect to saved WiFi...
WiFi connected!
IP address: 192.168.1.XXX
```

### Test 2.4: WiFi Connection Failure Handling

**Steps**:
1. Configure WiFi with wrong password
2. Reset ESP32

**Expected Result**:
```
Attempting to connect to saved WiFi...
.........
Failed to connect to saved WiFi.
Attempting to connect to default hotspot for provisioning...
Connected to default hotspot!
IP address: 192.168.x.x
Use MQTT to configure new WiFi credentials.
```

## 3. MQTT Manager Tests

### Test 3.1: MQTT Initial State (No Configuration)

**Steps**:
1. ESP32 connected to WiFi
2. Monitor serial output

**Expected Result**:
```
Initializing MQTT...
No MQTT configuration found
```

### Test 3.2: Configure MQTT via MQTT

**Setup**: ESP32 connected to WiFi, MQTT broker running

**Steps**:
```bash
# Note: This requires MQTT to already be configured once
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m '{
    "server": "localhost",
    "port": 1883
  }'
```

**Expected Result**:
- Serial monitor shows: "MQTT configuration updated via MQTT"
- Device publishes: "mqtt_config_updated" to status topic

### Test 3.3: MQTT Connection

**Steps**:
1. Configure MQTT (Test 3.2)
2. Restart ESP32
3. Monitor serial output

**Expected Result**:
```
Initializing MQTT...
MQTT configuration loaded
Attempting MQTT connection...connected
Subscribed to: esp32vault/ESP32-Vault-XXXXXXXX/cmd/#
```

### Test 3.4: MQTT Status Publishing

**Steps**:
1. ESP32 connected to MQTT
2. Subscribe to status topic:
```bash
mosquitto_sub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/status" \
  -v
```

**Expected Result**:
- Status message received every 30 seconds
- JSON format with device info:
```json
{
  "device_id": "XXXXXXXX",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "wifi_ssid": "YourNetwork",
  "ip_address": "192.168.1.100",
  "mqtt_connected": true,
  "ota_enabled": false
}
```

### Test 3.5: MQTT Reconnection

**Steps**:
1. ESP32 connected to MQTT
2. Stop MQTT broker
3. Wait 10 seconds
4. Start MQTT broker
5. Monitor serial output

**Expected Result**:
```
Attempting MQTT connection...failed, rc=-2 will try again in 5 seconds
Attempting MQTT connection...failed, rc=-2 will try again in 5 seconds
Attempting MQTT connection...connected
```

## 4. MQTT Command Tests

### Test 4.1: Restart Command

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/restart" \
  -m "1"
```

**Expected Result**:
- Serial monitor shows: "restarting"
- Device restarts
- Device reconnects to WiFi and MQTT

### Test 4.2: Reset WiFi Command

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/reset_wifi" \
  -m "1"
```

**Expected Result**:
- Serial monitor shows: "WiFi credentials cleared"
- Device publishes "wifi_reset" to status
- Device restarts and connects to default hotspot

### Test 4.3: Trigger OTA Update Command

**Prerequisites**: 
- Firmware binary file available on HTTP server
- Calculate SHA256 hash: `sha256sum firmware.bin`

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota_update" \
  -m '{
    "version": "1.0.2",
    "url": "http://your-server.com/firmware.bin",
    "integrity": "sha256:your-hash-here"
  }'
```

**Expected Result**:
- Serial monitor shows: "Starting OTA Update"
- Device publishes progress to `ota/status` topic
- Firmware downloads and flashes
- Device reboots with new firmware

**Monitor Progress**:
```bash
mosquitto_sub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/ota/status" -v
```

### Test 4.4: Update MQTT Configuration

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m '{
    "server": "new-broker.com",
    "port": 1883,
    "user": "testuser",
    "password": "testpass"
  }'
```

**Expected Result**:
- Serial monitor shows: "MQTT configuration saved"
- Device publishes "mqtt_config_updated" to status

## 5. OTA Update Tests

### Test 5.1: Build and Prepare Firmware Binary

**Steps**:
1. Make a small code change (e.g., add Serial.println("Version 1.0.2"))
2. Build project: `pio run`
3. Locate firmware binary: `.pio/build/esp32dev/firmware.bin`
4. Calculate SHA256 hash:
```bash
sha256sum .pio/build/esp32dev/firmware.bin
```
5. Host the binary on a web server (can be local HTTP server for testing)

### Test 5.2: HTTP OTA Update via MQTT

**Prerequisites**: 
- Firmware binary hosted on HTTP server
- ESP32 connected to WiFi and MQTT

**Steps**:
1. Subscribe to OTA status:
```bash
mosquitto_sub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/ota/status" -v
```

2. In another terminal, trigger OTA update:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota_update" \
  -m '{
    "version": "1.0.2",
    "url": "http://192.168.1.100:8000/firmware.bin",
    "integrity": "sha256:your-actual-hash"
  }'
```

**Expected Result**:
- Status messages published to `ota/status`:
  - `{"status":"starting","version":"1.0.2"}`
  - `{"status":"downloading"}`
  - `{"status":"updating","progress":10}` ... `{"progress":100}`
  - `{"status":"success","message":"Update completed, rebooting..."}`
- Serial monitor shows download progress
- Device reboots automatically
- New code executes (verify with serial output)

### Test 5.3: OTA Update with Invalid URL

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota_update" \
  -m '{
    "version": "1.0.3",
    "url": "http://invalid-server.com/firmware.bin"
  }'
```

**Expected Result**:
- Error message published to `ota/status`
- Serial monitor shows connection error
- Device continues normal operation (no crash)

### Test 5.4: OTA Update with Missing Fields

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota_update" \
  -m '{
    "version": "1.0.3"
  }'
```

**Expected Result**:
- Error message: "Missing required fields: version and url"
- Device continues normal operation

### Test 5.5: Concurrent OTA Update Prevention

**Steps**:
1. Start an OTA update
2. While update is in progress, send another OTA update command

**Expected Result**:
- Second command receives error: "Update already in progress"
- First update continues normally

## 6. Integration Tests

### Test 6.1: Full System Startup

**Steps**:
1. Flash firmware to ESP32
2. Configure WiFi credentials
3. Configure MQTT broker
4. Configure signal pins
5. Trigger OTA update (optional)

**Expected Result**:
- All features operational
- Signal telemetry publishing binary data
- Diagnostics reporting
- Commands responding
- OTA updates available on demand

### Test 6.2: Network Disconnection Recovery

**Steps**:
1. System fully operational
2. Disconnect WiFi router from internet
3. Wait 30 seconds
4. Reconnect WiFi router

**Expected Result**:
- WiFi reconnects automatically
- MQTT reconnects automatically
- No manual intervention needed

### Test 6.3: Long-term Stability

**Steps**:
1. System fully operational
2. Run for 24 hours
3. Monitor for crashes or memory leaks

**Expected Result**:
- No crashes or resets
- Free heap remains stable
- All features continue to work
- Status messages publish regularly

## 7. Performance Tests

### Test 7.1: Memory Usage

**Steps**:
1. Monitor status messages
2. Check `free_heap` value

**Expected Result**:
- Free heap > 80KB after initialization
- Free heap stable over time (no leaks)

### Test 7.2: Response Time

**Steps**:
1. Send MQTT command
2. Measure time to response

**Expected Result**:
- Command acknowledged < 1 second
- Action completed < 3 seconds

### Test 7.3: Concurrent Operations

**Steps**:
1. Send MQTT commands while OTA update in progress
2. Access web interface during MQTT communication

**Expected Result**:
- All operations complete successfully
- No interference between features

## 8. Error Handling Tests

### Test 8.1: Invalid WiFi Credentials

Covered in Test 2.5

### Test 8.2: Invalid MQTT Broker Address

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m '{
    "server": "invalid-broker-address.fake",
    "port": 1883
  }'
```

**Expected Result**:
- Serial monitor shows: "Attempting MQTT connection...failed"
- Device continues to retry
- Other features (WiFi, OTA) unaffected

### Test 8.3: Malformed JSON Command

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m 'invalid json {'
```

**Expected Result**:
- Serial monitor shows: "Failed to parse"
- Device continues normal operation
- No crashes

## 9. Security Tests

### Test 9.1: WiFi Credential Storage

**Steps**:
1. Configure WiFi
2. Flash new firmware via USB (not OTA)
3. Check if WiFi still works

**Expected Result**:
- WiFi credentials persist in NVS
- Device connects automatically
- Credentials encrypted in NVS

### Test 9.2: MQTT Authentication

**Steps**:
1. Configure MQTT with username/password
2. Try connecting without credentials

**Expected Result**:
- Connection fails without credentials
- Connection succeeds with correct credentials

## Test Checklist

Use this checklist to verify all tests:

### Build & Flash
- [ ] Build succeeds with ESP-IDF
- [ ] Flash via USB succeeds
- [ ] Serial monitor shows output

### WiFi Manager
- [ ] Default hotspot connection on first boot
- [ ] MQTT-based WiFi configuration works
- [ ] Reconnection after restart
- [ ] Failure handling works

### MQTT Manager
- [ ] MQTT connects with valid config
- [ ] Status messages publish
- [ ] Commands received and processed
- [ ] Auto-reconnection works

### MQTT Commands
- [ ] Restart command works
- [ ] Reset WiFi command works
- [ ] Trigger OTA update command works
- [ ] MQTT config update works

### OTA Updates
- [ ] Firmware binary builds successfully
- [ ] HTTP OTA update via MQTT succeeds
- [ ] OTA progress reporting works
- [ ] Invalid URL handling works
- [ ] Missing fields validation works
- [ ] Concurrent update prevention works

### Integration
- [ ] Full system startup
- [ ] Network recovery
- [ ] Long-term stability

### Performance
- [ ] Memory usage acceptable
- [ ] Response time good
- [ ] Concurrent operations work

### Error Handling
- [ ] Invalid credentials handled
- [ ] Invalid broker handled
- [ ] Malformed JSON handled

## Automated Testing (Future)

Consider implementing:
- Unit tests for manager classes
- Integration tests with mock MQTT broker
- Continuous integration pipeline
- Automated regression testing

## Reporting Issues

When reporting issues, include:
1. Test that failed
2. Expected vs actual result
3. Serial monitor output
4. ESP32 board type
5. ESP-IDF version (run `idf.py --version`)
6. Network configuration
