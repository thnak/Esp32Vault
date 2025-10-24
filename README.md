# ESP32 Vault

A comprehensive IoT solution for ESP32 with Arduino framework, featuring WiFi configuration, MQTT connectivity, and OTA updates.

## Features

### 1. WiFi Management
- **Automatic Connection**: Connects to saved WiFi credentials on startup
- **AP Configuration Mode**: If no credentials exist or connection fails, device starts as Access Point
- **Web-based Configuration**: Simple web interface to configure WiFi credentials
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

### 3. OTA (Over-The-Air) Updates
- **HTTP(S) OTA**: Firmware updates via HTTP/HTTPS download
- **MQTT-Controlled**: Triggered by MQTT commands with update URL
- **Integrity Verification**: Built-in binary verification (SHA256 support noted for future)
- **Progress Monitoring**: Real-time update progress feedback via MQTT

### 4. Configuration Management
- **Local Storage**: WiFi credentials stored locally using Preferences
- **Remote Configuration**: MQTT broker and OTA settings manageable via MQTT
- **Factory Reset**: WiFi credentials can be cleared remotely

## Project Structure

```
Esp32Vault/
├── platformio.ini          # PlatformIO configuration
├── include/                # Header files
│   ├── WiFiManager.h      # WiFi management
│   ├── MQTTManager.h      # MQTT client
│   └── OTAManager.h       # OTA updates
└── src/                   # Source files
    ├── main.cpp           # Main application
    ├── WiFiManager.cpp    # WiFi implementation
    ├── MQTTManager.cpp    # MQTT implementation
    └── OTAManager.cpp     # OTA implementation
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

1. On first boot, the device will start in AP mode
2. Connect to WiFi network: `ESP32-Vault-XXXXXXXX` (password: `12345678`)
3. Open browser and navigate to `http://192.168.4.1`
4. Enter your WiFi SSID and password
5. Click "Save & Connect"
6. Device will restart and connect to your WiFi network

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

## Dependencies

- **espressif32**: ESP32 platform
- **PubSubClient**: MQTT client library
- **ArduinoJson**: JSON parsing and generation

## Configuration Storage

The device uses ESP32 Preferences (NVS) to store:
- WiFi SSID and password (namespace: "wifi")
- MQTT broker configuration (namespace: "mqtt")

## Security Considerations

1. Change the default AP password in `WiFiManager.cpp`
2. Use secure MQTT connections (TLS) in production
3. Set OTA password using `otaManager.setPassword()` in `main.cpp`
4. Don't expose the AP mode to untrusted networks

## Troubleshooting

### Device won't connect to WiFi
- Check SSID and password are correct
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Try resetting WiFi credentials via MQTT or manually

### MQTT not connecting
- Verify MQTT broker address and port
- Check firewall settings
- Ensure credentials are correct if using authentication

### OTA update fails
- Ensure device and computer are on same network
- Check that sufficient flash space is available
- Verify network stability during update

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.