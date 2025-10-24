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
  - `esp32vault/{device_id}/cmd/io/#` - IO management topics
  - `esp32vault/{device_id}/io/{pin}/state` - Pin state reports

### 3. OTA (Over-The-Air) Updates
- **ArduinoOTA**: Built-in OTA update support
- **Network-based Updates**: Update firmware over WiFi
- **Progress Monitoring**: Real-time update progress feedback

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
- **Remote Configuration**: MQTT broker settings manageable via MQTT
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

### Enable OTA
```
Topic: esp32vault/{device_id}/cmd/ota
Payload: enable
```

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
  "ota_enabled": true
}
```

The device also publishes WiFi signal strength every 10 seconds to:
```
Topic: esp32vault/{device_id}/signal/strenght
Payload: -45
```

## OTA Updates

### Using Arduino IDE
1. Ensure device is connected to same network as your computer
2. In Arduino IDE, go to Tools > Port
3. Select the network port (ESP32-Vault-XXXXXXXX at x.x.x.x)
4. Upload your sketch

### Using PlatformIO
```bash
pio run --target upload --upload-port ESP32-Vault-XXXXXXXX.local
```

## IO Management Examples

For detailed IO management examples and use cases, see [IO_USAGE_EXAMPLES.md](IO_USAGE_EXAMPLES.md).

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