# ESP32 Vault - Signal Telemetry v1

A high-precision signal capture system for ESP32 with ESP-IDF framework, featuring WiFi configuration, MQTT connectivity, OTA updates, and professional-grade signal telemetry.

## Philosophy

> **Firmware = Oscilloscope | Server = Judge**

ESP32 Vault captures all signal changes with maximum accuracy and minimal processing, leaving all business logic and filtering to the server. This provides:

- **Maximum signal accuracy** - Every edge captured
- **Complete auditability** - All data preserved with sequence numbers
- **Flexibility** - Server logic can change without firmware updates
- **Replay capability** - Reconstruct exact signal timeline from captured data

## Features

### 1. Signal Telemetry (NEW!)

- **Raw Edge Capture**: Captures every GPIO level change without filtering or debouncing
- **Pulse Width Measurement**: Precise pulse timing using RMT peripheral or ISR
- **Binary Payloads**: Efficient packed binary format for signal data
- **Batching System**: Intelligent batching (up to 50 edges per packet, 50ms max window)
- **Flood Protection**: Priority-based queue with smart drop policies
- **PSRAM Offline Buffer**: 4MB PSRAM buffer for offline telemetry when MQTT is unavailable
- **Automatic Replay**: Buffered packets replayed in sequence order when connection restored
- **Diagnostic Monitoring**: Real-time tracking of drops, queue depth, and PSRAM buffer usage
- **Sequence Numbers**: Monotonic sequence for packet loss detection and reboot detection
- **Microsecond Timestamps**: High-resolution monotonic timing from hardware timer

### 2. WiFi Management
- **Automatic Connection**: Connects to saved WiFi credentials on startup
- **Default Hotspot Provisioning**: If no credentials exist or connection fails, device connects to predefined hotspot (SSID: `EspSetup`, Password: `HeLooWod`)
- **MQTT-based Configuration**: Configure WiFi credentials via MQTT commands
- **Persistent Storage**: WiFi credentials stored in ESP32 preferences

### 2. MQTT Integration (MQTT 5.0)
- **MQTT5 Protocol**: Full MQTT 5.0 support with properties
- **Content-Type Properties**: Automatic content type identification for all messages
- **Message Expiry**: Time-sensitive telemetry expires after 60 seconds
- **Binary & Text Support**: Supports both binary signal data and JSON commands
- **Payload Format Indicator**: Proper binary (0) and UTF-8 (1) format indicators
- **Auto-reconnection**: Automatic reconnection to MQTT broker
- **Dynamic Configuration**: MQTT settings can be configured via MQTT messages
- **MAC-based Client ID**: Uses device MAC address as MQTT client ID
- **Topic Structure**: 
  - `raw/{pin}` - Raw edge batches (binary, `application/vnd.esp32vault.signal.raw+bin`)
  - `pulse/{pin}` - Pulse width measurements (binary, `application/vnd.esp32vault.signal.pulse+bin`)
  - `diag` - Diagnostic data (binary, `application/vnd.esp32vault.signal.diag+bin`)
  - `heartbeat` - Device heartbeat (JSON, `application/json`)
  - `esp32vault/{mac}/status` - Device status and telemetry (JSON, `application/json`)
  - `esp32vault/{mac}/cmd/#` - Command topics

### 3. OTA (Over-The-Air) Updates
- **HTTP(S) OTA**: Firmware updates via HTTP/HTTPS download
- **MQTT-Controlled**: Triggered by MQTT commands with update URL
- **Integrity Verification**: Built-in binary verification (SHA256 support noted for future)
- **Progress Monitoring**: Real-time update progress feedback via MQTT

### 4. Signal Capture Architecture

- **RTOS-based Design**: Lock-free ring buffer with prioritized task system
- **ISR Safety**: Interrupt handlers copy to ring buffer without malloc
- **High Priority Collection**: Dedicated task for batching signals (priority 10)
- **Low Priority Publishing**: Separate MQTT publish task (priority 3)
- **No Filtering**: Captures all edges without debouncing or threshold logic
- **Deterministic**: Same input always produces same output for auditability

### 5. Configuration Management
- **Local Storage**: WiFi credentials stored locally using Preferences
- **Remote Configuration**: MQTT broker and OTA settings manageable via MQTT
- **Factory Reset**: WiFi credentials can be cleared remotely
- **Signal Pin Configuration**: Configure pins for raw edge or pulse width capture via MQTT

## Quick Start

### Signal Telemetry Example

1. Configure a pin to capture all edges:

```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 14,
    "capture_raw": true,
    "capture_pulse": false
  }'
```

2. Subscribe to raw edge data:

```bash
mosquitto_sub -h broker.example.com \
  -t "raw/14" -F "%t: %x" -v
```

3. Monitor diagnostics:

```bash
mosquitto_sub -h broker.example.com \
  -t "diag" -F "%t: %x" -v
```

For complete documentation, see:
- **[SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md)** - Complete signal telemetry documentation
- **[PSRAM_OFFLINE_BUFFER.md](PSRAM_OFFLINE_BUFFER.md)** - PSRAM offline buffering documentation
- **[MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)** - Migration from old InputManager system

## Project Structure

```
Esp32Vault/
├── CMakeLists.txt              # ESP-IDF root build file
├── sdkconfig.defaults          # ESP-IDF default configuration
├── main/                       # Main component (ESP-IDF)
│   ├── CMakeLists.txt         # Component build file
│   ├── main.cpp               # Application entry point (app_main)
│   ├── WiFiManager.h/cpp      # WiFi management (ESP-IDF)
│   ├── MQTTManager.h/cpp      # MQTT5 client (esp-mqtt)
│   ├── OTAManager.h/cpp       # OTA updates (esp_https_ota)
│   └── SignalTelemetry.h/cpp  # Signal capture system
└── build/                      # Build output (generated)
```

## Getting Started

### Prerequisites
- ESP-IDF v5.x or later
- ESP32 development board with PSRAM (4MB recommended)
- USB cable for initial programming

### Building and Uploading

1. Clone the repository:
```bash
git clone https://github.com/thnak/Esp32Vault.git
cd Esp32Vault
```

2. Set up ESP-IDF environment:
```bash
. $HOME/esp/esp-idf/export.sh
```

3. Build the project:
```bash
idf.py build
```

4. Flash to ESP32:
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

See [ESP_IDF_BUILD.md](ESP_IDF_BUILD.md) for detailed build instructions.

### Linux Host Testing (Experimental)

ESP32 Vault includes support for unit tests and demo applications that run on Linux without requiring ESP32 hardware:

```bash
# Run unit tests on Linux
./build_and_test_linux.sh

# Run demo application on Linux
./build_and_run_demo_linux.sh
```

See [test/host_test_linux/README.md](test/host_test_linux/README.md) and [demo/linux_demo/README.md](demo/linux_demo/README.md) for details.

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

**Note**: Replace `{mac}` with your device MAC address (e.g., `A0B1C2D3E4F5`). The MAC address is shown in serial monitor on boot.

### Signal Telemetry Commands

#### Configure Pin for Signal Capture

```json
Topic: esp32vault/{mac}/cmd/signal/config
Payload: {
  "pin": 14,
  "capture_raw": true,
  "capture_pulse": false,
  "use_rmt": false
}
```

**Parameters**:
- `pin`: GPIO pin number (0-39 on ESP32)
- `capture_raw`: Capture all edge changes (default: true)
- `capture_pulse`: Measure pulse widths (default: false)
- `use_rmt`: Use RMT peripheral for pulse measurement (default: false)

#### Remove Pin Configuration

```json
Topic: esp32vault/{mac}/cmd/signal/remove
Payload: {
  "pin": 14
}
```

### Device Management Commands

#### Configure MQTT Broker

```json
Topic: esp32vault/{mac}/cmd/mqtt
Payload: {
  "server": "broker.example.com",
  "port": 1883,
  "user": "username",
  "password": "password"
}
```

#### Update WiFi Credentials

```json
Topic: esp32vault/{mac}/cmd/wifi
Payload: {
  "ssid": "YourWiFiSSID",
  "password": "YourWiFiPassword"
}
```
Note: Device will restart after updating credentials.

#### Trigger OTA Update

```json
Topic: esp32vault/{mac}/cmd/ota_update
Payload: {
  "version": "1.0.2",
  "url": "http://example.com/firmware.bin",
  "integrity": "sha256:abcdef1234567890..."
}
```

Note: The `integrity` field is optional. SHA256 verification is noted for future enhancement.

#### Restart Device

```
Topic: esp32vault/{mac}/cmd/restart
Payload: any
```

#### Reset WiFi Credentials

```
Topic: esp32vault/{mac}/cmd/reset_wifi
Payload: any
```

## Published Topics (Device → Server)

### Signal Data (Binary)

```
raw/{pin}        - Raw edge change batches (binary packed format)
pulse/{pin}      - Pulse width measurements (binary packed format)
diag             - Diagnostic counters (binary packed format)
```

### Status Data (JSON)

```
heartbeat        - Device heartbeat every 30s
```

Example:
```json
{
  "mac": "A0B1C2D3E4F5",
  "seq": 12345,
  "uptime": 3600
}
```

```
esp32vault/{mac}/status  - Device status every 30s
```

Example:
```json
{
  "device_id": "A0B1C2D3E4F5",
  "uptime": 3600,
  "free_heap": 150000,
  "wifi_rssi": -45,
  "mqtt_connected": true,
  "firmware_version": "Signal Telemetry v1",
  "dropped_raw": 0,
  "dropped_pulse": 0,
  "queue_depth": 2,
  "psram_buffer_count": 0,
  "psram_buffer_dropped": 0,
  "psram_buffer_usage_pct": 0.0
}
```

## Binary Payload Format

All signal data uses binary packed format for efficiency. See [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md) for detailed specifications.

### Example: Parse Raw Edge Batch (Python)

```python
import struct

def parse_raw_packet(payload):
    # Unpack header
    version, packet_type = struct.unpack('BB', payload[0:2])
    
    # Unpack RawPacket
    base_time_us, base_seq, count = struct.unpack('<QIB', payload[2:15])
    
    # Unpack edges
    edges = []
    offset = 15
    for i in range(count):
        pin, value, dt_us = struct.unpack('<BBI', payload[offset:offset+6])
        edges.append({
            'pin': pin,
            'value': value,
            'time_us': base_time_us + dt_us,
            'seq': base_seq + i
        })
        offset += 6
    
    return edges
```

## Device Status

See "Published Topics" section above for status message formats.

During OTA updates, progress is published to:
```
Topic: esp32vault/{mac}/ota/status
Payload: {
  "status": "downloading|updating|success|error",
  "progress": 75,
  "version": "1.0.2",
  "message": "..."
}
```

## OTA Updates

OTA updates are performed via HTTP(S) and triggered through MQTT commands.

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
idf.py build

# The firmware binary will be at:
# build/esp32vault.bin
```

## Documentation

- **[SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md)** - Complete signal telemetry documentation
- **[PSRAM_OFFLINE_BUFFER.md](PSRAM_OFFLINE_BUFFER.md)** - PSRAM offline buffering documentation
- **[MQTT5_IMPLEMENTATION.md](MQTT5_IMPLEMENTATION.md)** - MQTT5 features and implementation guide
- **[ESP_IDF_BUILD.md](ESP_IDF_BUILD.md)** - Detailed ESP-IDF build instructions
- **[MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)** - Migration from old InputManager system
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture details

## Dependencies

- **ESP-IDF v5.x**: ESP32 development framework
- **esp-mqtt**: Native MQTT5 client (ESP-IDF component)
- **cJSON**: JSON parsing (ESP-IDF component)
- **FreeRTOS**: Real-time operating system (included with ESP-IDF)
- **ESP32 Hardware**: RMT peripheral, hardware timers, GPIO interrupts

## Configuration Storage

The device uses ESP32 Preferences (NVS) to store:
- WiFi SSID and password (namespace: "wifi")
- MQTT broker configuration (namespace: "mqtt")

## Security Considerations

1. Use secure MQTT connections (TLS) in production
2. Set OTA password using `otaManager.setPassword()` in `main.cpp`
3. Keep firmware updated
4. Change default hotspot credentials by modifying `DEFAULT_SSID` and `DEFAULT_PASSWORD` in `WiFiManager.h` for enhanced security
5. Binary payloads should be validated on the server side
6. Implement rate limiting on the server to prevent abuse

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

### High dropped_raw counter
- Too many edges for current batch/publish rate
- Reduce number of configured pins
- Ensure stable network connection
- Check MQTT broker performance
- Check PSRAM buffer status (may be full)

### No data on raw/ topics
- Verify pin is configured (check status message)
- Ensure pin has signal activity
- Check MQTT connection
- Subscribe to correct topic: `raw/{pinId}`
- If offline, data may be in PSRAM buffer awaiting replay

### PSRAM buffer issues
- Check device has PSRAM installed (4MB recommended)
- Verify PSRAM configuration in sdkconfig
- Monitor `psram_buffer_count` and `psram_buffer_dropped` in status
- See [PSRAM_OFFLINE_BUFFER.md](PSRAM_OFFLINE_BUFFER.md) for details

## Performance Notes

### Timing Accuracy
- ISR latency: < 5 microseconds (typical)
- Timestamp resolution: 1 microsecond
- Batch publish latency: < 50ms typical

### Throughput
- Max edges/second: ~10,000 (with batching)
- Max batch rate: 20 batches/second
- MQTT payload size: 50-500 bytes per batch typically

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.