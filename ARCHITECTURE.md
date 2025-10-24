# ESP32 Vault Architecture

## System Overview

ESP32 Vault is designed as a modular IoT solution with three main components working together to provide a robust and configurable system.

```
┌──────────────────────────────────────────────────────────────────┐
│                         ESP32 Device                             │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │              │  │              │  │              │         │
│  │ WiFiManager  │  │ MQTTManager  │  │ OTAManager   │         │
│  │              │  │              │  │              │         │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘         │
│         │                  │                  │                 │
│         │         ┌────────┴────────┐         │                 │
│         │         │                 │         │                 │
│         │    ┌────▼─────┐    ┌──────▼────┐   │                 │
│         │    │          │    │           │   │                 │
│         │    │InputMgr  │    │  main.cpp │   │                 │
│         │    │(FreeRTOS)│◄───┤Integration│   │                 │
│         │    │          │    │           │   │                 │
│         │    └──────────┘    └───────────┘   │                 │
│         │                           │         │                 │
│         └───────────────────────────┴─────────┘                 │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

## Component Descriptions

### 1. WiFiManager

**Purpose**: Manages WiFi connectivity with automatic fallback to AP mode for configuration.

**Features**:
- Persistent credential storage (ESP32 Preferences/NVS)
- Automatic connection to saved networks
- AP mode with web-based configuration portal
- Connection timeout and retry logic

**States**:
- `Station Mode (STA)`: Connected to WiFi network
- `Access Point Mode (AP)`: Hosting configuration portal

**Flow**:
```
Start
  │
  ├─→ Load saved credentials?
  │   ├─→ Yes → Connect to WiFi
  │   │   ├─→ Success → Station Mode
  │   │   └─→ Fail → Start AP Mode
  │   └─→ No → Start AP Mode
  │
  └─→ AP Mode
      └─→ User configures → Save & Restart
```

### 2. MQTTManager

**Purpose**: Handles all MQTT communication including connection, subscription, and publishing.

**Features**:
- Auto-reconnection with exponential backoff
- Dynamic topic subscription
- Persistent MQTT broker configuration
- Callback system for message handling

**Topic Structure**:
```
esp32vault/
  └── {device_id}/
      ├── status        (published by device)
      ├── config        (published by device)
      ├── io/
      │   └── {pin}/
      │       └── state (published - pin state)
      ├── ota/
      │   └── status    (published by device - OTA progress)
      └── cmd/
          ├── mqtt      (subscribed - configure broker)
          ├── ota       (subscribed - enable OTA)
          ├── ota_update (subscribed - trigger OTA update)
          ├── restart   (subscribed - restart device)
          ├── reset_wifi (subscribed - reset WiFi)
          └── io/
              ├── config    (subscribed - configure pin)
              ├── exclude   (subscribed - set exclusion list)
              └── {pin}/
                  └── trigger (subscribed - trigger output)

```

### 3. OTAManager

**Purpose**: Enables Over-The-Air firmware updates via HTTP(S).

**Features**:
- HTTP(S)-based firmware downloads
- MQTT-triggered updates
- Real-time progress monitoring via MQTT
- Automatic binary verification
- Error handling and recovery
- Support for firmware integrity checking (SHA256 noted for future)

**Update Process**:
```
MQTT Command Received (cmd/ota_update)
  │
  ├─→ Parse JSON payload (version, url, integrity)
  │   └─→ Validate parameters
  │
  ├─→ Download firmware via HTTP(S)
  │   ├─→ Report progress via MQTT (every 10%)
  │   └─→ Verify binary format
  │
  ├─→ Flash to memory
  │   └─→ Verify flash operation
  │
  └─→ Reboot with new firmware
```

### 4. InputManager

**Purpose**: Manages dynamic GPIO pin configuration and monitoring via MQTT.

**Features**:
- Dynamic pin configuration (input, output, analog, interrupt)
- ISR-safe event queue using FreeRTOS
- Pin exclusion management for critical pins
- Debounce support for inputs
- Remote trigger operations (set, reset, pulse, toggle)
- Automatic state reporting to MQTT
- Persistent configuration storage in NVS

**Architecture**:
```
┌─────────────────────────────────────────┐
│         InputManager                    │
│                                         │
│  ┌──────────────┐  ┌───────────────┐  │
│  │ Pin Configs  │  │ Exclude List  │  │
│  │   (NVS)      │  │    (NVS)      │  │
│  └──────────────┘  └───────────────┘  │
│                                         │
│  ┌──────────────────────────────────┐  │
│  │   FreeRTOS Event Queue           │  │
│  │   (ISR-safe, overwrite oldest)   │  │
│  └──────────────────────────────────┘  │
│              │                          │
│              ▼                          │
│  ┌──────────────────────────────────┐  │
│  │   Worker Task                    │  │
│  │   - Process events               │  │
│  │   - Apply debounce               │  │
│  │   - Publish to MQTT              │  │
│  └──────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

**Flow**:
```
GPIO Interrupt
  │
  ├─→ ISR Handler (IRAM_ATTR)
  │   └─→ Queue Event (ISR-safe)
  │       └─→ If full, remove oldest
  │
  └─→ Worker Task receives event
      ├─→ Apply debounce filter
      ├─→ Update pin state
      └─→ Publish to MQTT topic
```

## Data Flow

### Startup Sequence

```
1. Serial Begin (115200)
2. WiFiManager.begin()
   ├─→ Load credentials
   ├─→ Attempt connection
   └─→ Start AP or continue
3. If WiFi connected:
   ├─→ MQTTManager.begin()
   │   ├─→ Load MQTT config
   │   └─→ Connect to broker
   └─→ OTAManager.begin()
       └─→ Enable OTA updates
4. Enter main loop
```

### Main Loop

```
Loop:
  │
  ├─→ WiFiManager.loop()
  │   └─→ Handle web server requests (if AP mode)
  │
  ├─→ If WiFi connected and not in AP mode:
  │   ├─→ MQTTManager.loop()
  │   │   ├─→ Handle MQTT messages
  │   │   └─→ Reconnect if needed
  │   │
  │   ├─→ OTAManager.loop()
  │   │   └─→ Handle OTA requests
  │   │
  │   └─→ Periodic status publish (every 30s)
  │       └─→ Device telemetry
  │
  └─→ Delay 10ms
```

## Configuration Storage

### ESP32 Preferences (NVS)

**Namespace: "wifi"**
- `ssid`: WiFi network name
- `password`: WiFi password

**Namespace: "mqtt"**
- `server`: MQTT broker address
- `port`: MQTT broker port (default: 1883)
- `user`: MQTT username (optional)
- `password`: MQTT password (optional)

**Namespace: "io"**
- `pins`: JSON array of pin configurations
- `exclude`: JSON object with excluded pins and ranges

## Communication Protocols

### HTTP (Web Configuration)

- **Port**: 80
- **Endpoints**:
  - `GET /` - Configuration page
  - `POST /save` - Save WiFi credentials
  - `GET /status` - Device status

### MQTT

- **Protocol**: MQTT v3.1.1
- **Default Port**: 1883
- **QoS**: 0 (default)
- **Retained Messages**: Status messages

### OTA

- **Protocol**: HTTP(S) download + ESP32 Update library
- **Trigger**: MQTT command-based
- **Authentication**: Server-side (HTTP Basic Auth, API keys, etc.)
- **Progress Reporting**: Via MQTT publish

## Security Considerations

### Current Implementation
- WiFi credentials stored in encrypted NVS
- Optional MQTT authentication
- Default AP password (should be changed)

### Recommended Enhancements
1. **TLS/SSL**: Enable secure MQTT (port 8883) and HTTPS for OTA
2. **Certificate Validation**: Verify broker and firmware server certificates
3. **SHA256 Verification**: Implement full SHA256 integrity checking for OTA
4. **Unique AP Password**: Generate unique AP password per device
5. **Message Encryption**: Encrypt sensitive MQTT payloads
6. **OTA Authorization**: Implement token-based authorization for firmware downloads

## Extensibility

### Adding New Features

**Example: Add Temperature Sensor**

1. Create new manager class:
```cpp
class SensorManager {
    void begin();
    void loop();
    float readTemperature();
};
```

2. Integrate in main.cpp:
```cpp
SensorManager sensorManager;

void setup() {
    // ... existing code ...
    sensorManager.begin();
}

void loop() {
    // ... existing code ...
    sensorManager.loop();
}
```

3. Publish data via MQTT:
```cpp
void publishSensorData() {
    float temp = sensorManager.readTemperature();
    String topic = baseTopic + "/sensors/temperature";
    mqttManager.publish(topic, String(temp));
}
```

### Adding New MQTT Commands

1. Add handler in `handleMQTTMessage()`:
```cpp
else if (topic.endsWith("/cmd/your_command")) {
    // Handle your command
}
```

2. Document in README and example_mqtt_commands.md

## Performance Characteristics

### Memory Usage
- **Flash**: ~1.2MB (with libraries)
- **RAM**: ~100KB free heap after initialization
- **NVS**: <1KB for configuration storage

### Network
- **WiFi Reconnect**: ~5-10 seconds
- **MQTT Reconnect**: 5 seconds between attempts
- **Status Update**: Every 30 seconds
- **Web Server**: Handles 1 concurrent connection

### Power Consumption
- **Active Mode**: ~160mA @ 3.3V
- **Light Sleep**: ~20mA @ 3.3V (not implemented)
- **Deep Sleep**: ~10μA @ 3.3V (not implemented)

## Error Handling

### WiFi Connection Failure
- Fallback to AP mode
- Web portal for reconfiguration
- Persistent storage prevents repeated failures

### MQTT Connection Failure
- Automatic reconnection with backoff
- Continues operation without MQTT
- Logs errors to serial console

### OTA Update Failure
- Rollback to previous firmware
- Error logging
- Device continues normal operation

## Development Workflow

```
Development → Build → Upload → Test → Deploy
     │          │        │       │        │
     │          │        └───────┴────────┘
     │          │              OTA
     │          │
     │          └─→ Syntax check
     │              Library resolution
     │              Binary generation
     │
     └─→ Code → Compile → Debug
```

## Future Enhancements

1. **Web Dashboard**: Add web interface for monitoring
2. **Secure Boot**: Enable ESP32 secure boot
3. **Data Logging**: Local storage with SPIFFS/LittleFS
4. **Multiple WiFi**: Support multiple WiFi networks
5. **Power Management**: Add sleep modes for battery operation
6. **Sensor Integration**: Built-in sensor support
7. **Home Assistant**: MQTT Discovery for Home Assistant
8. **Time Sync**: NTP time synchronization
