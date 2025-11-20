# ESP32 Vault

A comprehensive IoT solution for ESP32 with Arduino framework, featuring WiFi configuration, MQTT connectivity, and OTA updates.

## Features

### 1. WiFi Management
- **Automatic Connection**: Connects to saved WiFi credentials on startup
- **Default Hotspot Provisioning**: If no credentials exist or connection fails, device connects to predefined hotspot (SSID: `EspSetup`, Password: `HeLooWod`)
- **MQTT-based Configuration**: Configure WiFi credentials via MQTT commands
- **Persistent Storage**: WiFi credentials stored in ESP32 preferences

### 2. MQTT Integration
- **PubSubClient Library**: Reliable MQTT communication
- **Auto-reconnection**: Automatic reconnection to MQTT broker
- **Dynamic Configuration**: MQTT settings can be configured via MQTT messages
- **Topic Structure**: 
  - `esp32vault/{device_id}/status` - Device status and telemetry
  - `esp32vault/{device_id}/signal/strenght` - WiFi signal strength (RSSI)
  - `esp32vault/{device_id}/config` - Configuration data
  - `esp32vault/{device_id}/cmd/#` - Command topics
  - `esp32vault/{device_id}/cmd/io/#` - IO management topics
  - `esp32vault/{device_id}/io/{pin}/state` - Pin state reports

### 3. OTA (Over-The-Air) Updates
- **HTTP(S) OTA**: Firmware updates via HTTP/HTTPS download
- **MQTT-Controlled**: Triggered by MQTT commands with update URL
- **Integrity Verification**: Built-in binary verification (SHA256 support noted for future)
- **Progress Monitoring**: Real-time update progress feedback via MQTT

### 4. Dynamic IO Management
- **Remote Pin Configuration**: Configure GPIO pins (input, output, analog, interrupt) via MQTT
- **Pin Exclusion**: Server-managed exclusion list for protecting critical pins
- **ISR-safe Event Queue**: FreeRTOS-based event queue for interrupt handling
- **Trigger Operations**: Remote trigger for output pins (set, reset, pulse, toggle)
- **State Reporting**: Automatic pin state reporting to configurable MQTT topics
- **Persistent Configuration**: Pin configurations saved to NVS (optional)
- **Debouncing**: Built-in debounce support for inputs and interrupts

### 5. Configuration Management
- **Local Storage**: WiFi credentials stored locally using Preferences
- **Remote Configuration**: MQTT broker and OTA settings manageable via MQTT
- **Factory Reset**: WiFi credentials can be cleared remotely
- **IO Configuration**: Pin configurations and exclude lists persisted to NVS

## Project Structure

```
Esp32Vault/
├── platformio.ini          # PlatformIO configuration
├── include/                # Header files
│   ├── WiFiManager.h      # WiFi management
│   ├── MQTTManager.h      # MQTT client
│   ├── OTAManager.h       # OTA updates
│   └── InputManager.h     # IO management
└── src/                   # Source files
    ├── main.cpp           # Main application
    ├── WiFiManager.cpp    # WiFi implementation
    ├── MQTTManager.cpp    # MQTT implementation
    ├── OTAManager.cpp     # OTA implementation
    └── InputManager.cpp   # IO implementation
```

## Getting Started

### Prerequisites
- PlatformIO IDE or PlatformIO Core
- ESP32 development board
- USB cable for initial programming

### Building and Uploading

1. Clone the repository:
```bash
git clone https://github.com/thnak/Esp32Vault.git
cd Esp32Vault
```

2. Build the project:
```bash
pio run
```

3. Upload to ESP32:
```bash
pio run --target upload
```

4. Monitor serial output:
```bash
pio device monitor
```

## Initial Setup

### WiFi Configuration

#### First Boot Setup

1. Set up a WiFi hotspot on your laptop or mobile device:
   - **SSID**: `EspSetup`
   - **Password**: `HeLooWod`
   - **Network Type**: 2.4GHz (ESP32 doesn't support 5GHz)

2. Power on the ESP32 device - it will automatically attempt to connect to the `EspSetup` hotspot

3. Start an MQTT broker on the device hosting the hotspot (your laptop):
   ```bash
   # Install mosquitto (if not already installed)
   # On Ubuntu/Debian:
   sudo apt-get install mosquitto mosquitto-clients
   
   # On macOS:
   brew install mosquitto
   
   # Start mosquitto broker
   mosquitto -v
   ```

4. Configure the MQTT broker on the ESP32:
   ```bash
   # Replace ESP32-Vault-XXXXXXXX with your device ID (shown in serial monitor)
   mosquitto_pub -h localhost \
     -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
     -m '{
       "server": "192.168.x.x",
       "port": 1883
     }'
   ```
   Note: Replace `192.168.x.x` with your laptop's IP address on the hotspot network

5. Configure your permanent WiFi credentials via MQTT:
   ```bash
   mosquitto_pub -h localhost \
     -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/wifi" \
     -m '{
       "ssid": "YourHomeWiFi",
       "password": "YourWiFiPassword"
     }'
   ```

6. The device will restart and connect to your configured WiFi network

#### Updating WiFi Credentials

To update WiFi credentials later via MQTT:
```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/wifi" \
  -m '{
    "ssid": "NewWiFiSSID",
    "password": "NewWiFiPassword"
  }'
```

### MQTT Configuration

Once connected to WiFi, configure MQTT by publishing to:
```
Topic: esp32vault/{device_id}/cmd/mqtt
Payload: {
  "server": "mqtt.example.com",
  "port": 1883,
  "user": "username",
  "password": "password"
}
```

## MQTT Commands

### Configure MQTT Broker
```json
Topic: esp32vault/{device_id}/cmd/mqtt
Payload: {
  "server": "broker.example.com",
  "port": 1883,
  "user": "username",
  "password": "password"
}
```

### Update WiFi Credentials
```json
Topic: esp32vault/{device_id}/cmd/wifi
Payload: {
  "ssid": "YourWiFiSSID",
  "password": "YourWiFiPassword"
}
```
Note: Device will restart after updating credentials.

### Trigger OTA Update
```json
Topic: esp32vault/{device_id}/cmd/ota_update
Payload: {
  "version": "1.0.2",
  "url": "http://example.com/firmware.bin",
  "integrity": "sha256:abcdef1234567890..."
}
```

Note: The `integrity` field is optional. SHA256 verification is noted for future enhancement. The ESP32 Update library provides built-in binary format verification.

### Restart Device
```
Topic: esp32vault/{device_id}/cmd/restart
Payload: any
```

### Reset WiFi Credentials
```
Topic: esp32vault/{device_id}/cmd/reset_wifi
Payload: any
```

### Update Configuration
```json
Topic: esp32vault/{device_id}/config/set
Payload: {
  "status_interval": 30000
}
```

### Configure IO Pin
```json
Topic: esp32vault/{device_id}/cmd/io/config
Payload: {
  "pin": 13,
  "mode": "output",
  "report_topic": "esp32vault/{device_id}/io/13/state",
  "persist": true,
  "retain": false
}
```

Modes: `output`, `input`, `input_pullup`, `analog`, `interrupt`

For interrupt mode, additional parameters:
```json
{
  "pin": 14,
  "mode": "interrupt",
  "edge": "change",
  "debounce": 50,
  "report_topic": "esp32vault/{device_id}/io/14/state",
  "persist": true
}
```

Edge types: `rising`, `falling`, `change`

### Trigger Output Pin
```json
Topic: esp32vault/{device_id}/cmd/io/13/trigger
Payload: set
```

Actions: `set` (HIGH), `reset` (LOW), `pulse`, `toggle`

For pulse action with custom duration:
```json
Payload: {
  "action": "pulse",
  "pulse": 500
}
```

### Set Pin Exclusion List
```json
Topic: esp32vault/{device_id}/cmd/io/exclude
Payload: {
  "pins": [0, 1, 3],
  "ranges": [{"from": 6, "to": 11}],
  "persist": true
}
```

## Device Status

The device publishes status every 30 seconds to:
```
Topic: esp32vault/{device_id}/status
Payload: {
  "device_id": "XXXXXXXX",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "wifi_ssid": "YourNetwork",
  "ip_address": "192.168.1.100",
  "mqtt_connected": true,
  "ota_update_in_progress": false
}
```

The device also publishes WiFi signal strength every 10 seconds to:
```
Topic: esp32vault/{device_id}/signal/strenght
Payload: -45
```

During OTA updates, progress is published to:
```
Topic: esp32vault/{device_id}/ota/status
Payload: {
  "status": "downloading|updating|success|error",
  "progress": 75,
  "version": "1.0.2",
  "message": "..."
}
```

## OTA Updates

OTA updates are now performed via HTTP(S) and triggered through MQTT commands. This provides better security and flexibility compared to the previous ArduinoOTA implementation.

### Performing an OTA Update

1. **Host your firmware**: Upload your compiled `.bin` firmware file to an HTTP(S) server
2. **Send MQTT command**: Publish to the OTA update topic:

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/{device_id}/cmd/ota_update" \
  -m '{
    "version": "1.0.2",
    "url": "http://example.com/firmware/esp32vault-v1.0.2.bin",
    "integrity": "sha256:your-sha256-hash-here"
  }'
```

3. **Monitor progress**: Subscribe to the OTA status topic:

```bash
mosquitto_sub -h your-broker.com \
  -t "esp32vault/{device_id}/ota/status" -v
```

The device will:
- Download the firmware from the specified URL
- Verify the binary format
- Flash the new firmware
- Automatically reboot with the new version

### Building Firmware Binary

To create a firmware binary for OTA updates:

```bash
# Build the project
pio run

# The firmware binary will be at:
# .pio/build/esp32dev/firmware.bin
```

## IO Management Examples

For detailed IO management examples and use cases, see [IO_USAGE_EXAMPLES.md](IO_USAGE_EXAMPLES.md).

Quick start guide: [IO_QUICKSTART.md](IO_QUICKSTART.md)

To test the implementation, run the provided test script:
```bash
# Set environment variables
export MQTT_BROKER="your-broker.com"
export DEVICE_ID="ESP32-Vault-XXXXXXXX"

# Run test
./test_io_management.sh
```

## Dependencies

- **espressif32**: ESP32 platform
- **PubSubClient**: MQTT client library
- **ArduinoJson**: JSON parsing and generation
- **FreeRTOS**: Real-time operating system (included with ESP32)

## Configuration Storage

The device uses ESP32 Preferences (NVS) to store:
- WiFi SSID and password (namespace: "wifi")
- MQTT broker configuration (namespace: "mqtt")

## Security Considerations

1. Use secure MQTT connections (TLS) in production
2. Set OTA password using `otaManager.setPassword()` in `main.cpp`
3. Keep firmware updated
4. Change default hotspot credentials by modifying `DEFAULT_SSID` and `DEFAULT_PASSWORD` in `WiFiManager.h` for enhanced security

## Troubleshooting

### Device won't connect to WiFi
- Check SSID and password are correct
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Try resetting WiFi credentials via MQTT or manually
- Verify the default hotspot (EspSetup) is available for initial provisioning

### MQTT not connecting
- Verify MQTT broker address and port
- Check firewall settings
- Ensure credentials are correct if using authentication

### OTA update fails
- Ensure device and update server are network accessible
- Check that sufficient flash space is available
- Verify network stability during update

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.