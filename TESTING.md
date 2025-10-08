# Testing Guide for ESP32 Vault

This document provides comprehensive testing procedures for ESP32 Vault.

## Prerequisites

- ESP32 development board connected to computer
- PlatformIO installed
- MQTT broker accessible (e.g., Mosquitto)
- WiFi network (2.4 GHz)
- MQTT client tools (mosquitto_pub, mosquitto_sub, or MQTT Explorer)

## 1. Build and Upload Test

### Test: Successful Build

```bash
cd Esp32Vault
pio run
```

**Expected Result**: 
- Build completes without errors
- Output shows: `SUCCESS`
- Firmware size displayed (should be < 1.5MB)

### Test: Upload Firmware

```bash
pio run --target upload
```

**Expected Result**:
- Upload completes successfully
- Device resets automatically
- Serial monitor shows startup messages

## 2. WiFi Manager Tests

### Test 2.1: First Boot (No Credentials)

**Steps**:
1. Upload firmware to fresh ESP32 (or after clearing NVS)
2. Monitor serial output

**Expected Result**:
```
ESP32 Vault Starting...
Initializing WiFi...
No saved credentials. Starting AP mode...
AP Mode started
SSID: ESP32-Vault-XXXXXXXX
IP address: 192.168.4.1
Web server started on port 80
```

### Test 2.2: AP Mode Web Interface

**Steps**:
1. Connect to WiFi network: `ESP32-Vault-XXXXXXXX` (password: `12345678`)
2. Open browser to `http://192.168.4.1`

**Expected Result**:
- Web page loads with title "ESP32 Vault"
- Form with SSID and Password fields visible
- "Save & Connect" button present

### Test 2.3: WiFi Configuration via Web

**Steps**:
1. In web interface, enter valid WiFi credentials
2. Click "Save & Connect"

**Expected Result**:
- Success page displayed
- Device restarts after 2 seconds
- Device connects to WiFi network
- Serial monitor shows:
  ```
  WiFi connected!
  IP address: 192.168.1.XXX
  ```

### Test 2.4: WiFi Reconnection After Restart

**Steps**:
1. Configure WiFi (Test 2.3)
2. Reset ESP32 (press reset button)
3. Monitor serial output

**Expected Result**:
```
Attempting to connect to saved WiFi...
WiFi connected!
IP address: 192.168.1.XXX
```

### Test 2.5: WiFi Connection Failure Handling

**Steps**:
1. Configure WiFi with wrong password
2. Reset ESP32

**Expected Result**:
```
Attempting to connect to saved WiFi...
.........
Failed to connect. Starting AP mode...
AP Mode started
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
- Device restarts in AP mode

### Test 4.3: Enable OTA Command

**Steps**:
```bash
mosquitto_pub -h localhost \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota" \
  -m "enable"
```

**Expected Result**:
- Serial monitor shows: "OTA Ready"
- Device publishes "ota_enabled" to status
- OTA hostname announced on network

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

### Test 5.1: OTA Discovery

**Prerequisites**: OTA enabled (Test 4.3)

**Steps**:
```bash
# List available OTA devices
pio device list --mdns
```

**Expected Result**:
- Device appears in list: `ESP32-Vault-XXXXXXXX.local`

### Test 5.2: OTA Firmware Update

**Steps**:
1. Make a small code change (e.g., add Serial.println)
2. Build project: `pio run`
3. Upload via OTA:
```bash
pio run --target upload --upload-port ESP32-Vault-XXXXXXXX.local
```

**Expected Result**:
```
Start updating sketch
Progress: 100%
End
```
- Device restarts with new firmware
- New code executes (check serial monitor)

### Test 5.3: OTA Update Failure Handling

**Steps**:
1. Start OTA update
2. Disconnect ESP32 from power during upload (simulate failure)
3. Reconnect power

**Expected Result**:
- Device boots with previous firmware (rollback)
- No boot loops
- Device operates normally

## 6. Integration Tests

### Test 6.1: Full System Startup

**Steps**:
1. Upload firmware to fresh ESP32
2. Configure WiFi via web interface
3. Configure MQTT via MQTT command
4. Enable OTA via MQTT command

**Expected Result**:
- All features operational
- Status messages publishing
- Commands responding
- OTA available

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
2. Upload new firmware via USB (not OTA)
3. Check if WiFi still works

**Expected Result**:
- WiFi credentials persist
- Device connects automatically
- Credentials encrypted in NVS

### Test 9.2: MQTT Authentication

**Steps**:
1. Configure MQTT with username/password
2. Try connecting without credentials

**Expected Result**:
- Connection fails without credentials
- Connection succeeds with credentials

## Test Checklist

Use this checklist to verify all tests:

### Build & Upload
- [ ] Build succeeds
- [ ] Upload via USB succeeds
- [ ] Serial monitor shows output

### WiFi Manager
- [ ] AP mode starts on first boot
- [ ] Web interface accessible
- [ ] WiFi configuration works
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
- [ ] Enable OTA command works
- [ ] MQTT config update works

### OTA Updates
- [ ] OTA device discovered
- [ ] Firmware update succeeds
- [ ] OTA failure handling

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
5. PlatformIO version
6. Network configuration
