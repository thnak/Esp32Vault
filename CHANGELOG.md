# Changelog

All notable changes to ESP32 Vault will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2024-10-08

### Added

#### Core Features
- **WiFi Management System**
  - Automatic connection to saved WiFi networks
  - Fallback to Access Point (AP) mode when connection fails
  - Web-based configuration portal for WiFi setup
  - Persistent credential storage using ESP32 Preferences (NVS)
  - Auto-reconnection with timeout handling

- **MQTT Integration**
  - Full MQTT client implementation using PubSubClient library
  - Auto-reconnection with exponential backoff
  - Dynamic MQTT broker configuration via MQTT messages
  - Command topic subscription for remote control
  - Status publishing with device telemetry
  - Configurable QoS and retained messages

- **OTA (Over-The-Air) Updates**
  - Network-based firmware updates using ArduinoOTA
  - Progress monitoring and error handling
  - mDNS hostname support
  - Optional password protection

- **Configuration Management**
  - Local storage of WiFi credentials
  - MQTT broker settings (server, port, credentials)
  - Remote configuration via MQTT
  - Factory reset capability

#### Project Structure
- **platformio.ini**: PlatformIO configuration for ESP32
- **include/**: Header files for all manager classes
  - WiFiManager.h
  - MQTTManager.h
  - OTAManager.h
- **src/**: Implementation files
  - main.cpp (application entry point)
  - WiFiManager.cpp
  - MQTTManager.cpp
  - OTAManager.cpp

#### Documentation
- **README.md**: Comprehensive project documentation
  - Feature descriptions
  - Getting started guide
  - MQTT command reference
  - Device status format
  - Troubleshooting section

- **QUICKSTART.md**: Step-by-step setup guide
  - Build and upload instructions
  - WiFi configuration steps
  - MQTT setup examples
  - Common issues and solutions

- **ARCHITECTURE.md**: Technical architecture documentation
  - System overview and component descriptions
  - Data flow diagrams
  - Configuration storage details
  - Security considerations
  - Performance characteristics

- **example_mqtt_commands.md**: MQTT usage examples
  - Command examples for all features
  - Python and Node.js code samples
  - Topic structure reference
  - Security best practices

- **CHANGELOG.md**: This file

#### Development Tools
- **.gitignore**: Build artifacts and IDE files exclusion
- PlatformIO project configuration with ESP32 platform

#### MQTT Topics
- `esp32vault/{device_id}/status` - Device status and telemetry
- `esp32vault/{device_id}/config` - Configuration data
- `esp32vault/{device_id}/cmd/mqtt` - Configure MQTT settings
- `esp32vault/{device_id}/cmd/ota` - Enable/disable OTA
- `esp32vault/{device_id}/cmd/restart` - Restart device
- `esp32vault/{device_id}/cmd/reset_wifi` - Reset WiFi credentials
- `esp32vault/{device_id}/config/set` - Update device configuration

#### Libraries
- PubSubClient v2.8+ for MQTT
- ArduinoJson v6.21+ for JSON parsing
- Built-in ESP32 libraries (WiFi, WebServer, Preferences, ArduinoOTA)

### Technical Specifications

#### Memory Usage
- Flash: ~1.2MB (including libraries)
- RAM: ~100KB free heap after initialization
- NVS: <1KB for configuration storage

#### Network Features
- WiFi: 802.11 b/g/n (2.4 GHz)
- MQTT: v3.1.1 protocol support
- HTTP: Web server on port 80 (AP mode)
- OTA: Port 3232 (ArduinoOTA default)

#### Supported Platforms
- ESP32 (all variants)
- Framework: Arduino
- Build System: PlatformIO

### Security Features
- WiFi credentials encrypted in NVS storage
- Optional MQTT authentication
- Optional OTA password protection
- Secure credential storage in ESP32 flash

### Performance
- WiFi reconnection: ~5-10 seconds
- MQTT reconnection: 5 seconds between attempts
- Status updates: Every 30 seconds (configurable)
- Web server: Single concurrent connection

## Future Roadmap

### Planned Features
- [ ] TLS/SSL support for secure MQTT
- [ ] Home Assistant MQTT Discovery
- [ ] Web dashboard for monitoring
- [ ] Multiple WiFi network support
- [ ] Deep sleep power management
- [ ] Time synchronization (NTP)
- [ ] Local data logging (SPIFFS/LittleFS)
- [ ] Sensor integration framework
- [ ] REST API for configuration
- [ ] Captive portal for easier AP setup

### Under Consideration
- [ ] Bluetooth configuration
- [ ] LoRaWAN support
- [ ] WebSocket support
- [ ] MQTT message queue during disconnection
- [ ] Firmware rollback on OTA failure
- [ ] Watchdog timer implementation
- [ ] Configuration backup/restore

## Migration Guide

### From Fresh Installation
This is the initial release. No migration needed.

## Credits

### Built With
- [PlatformIO](https://platformio.org/) - Build system and IDE
- [ESP-IDF](https://github.com/espressif/esp-idf) - ESP32 development framework
- [Arduino Core for ESP32](https://github.com/espressif/arduino-esp32) - Arduino framework
- [PubSubClient](https://github.com/knolleary/pubsubclient) - MQTT library
- [ArduinoJson](https://arduinojson.org/) - JSON library

## License

This project is licensed under the MIT License - see the LICENSE file for details.

---

**Note**: Version numbers follow Semantic Versioning (MAJOR.MINOR.PATCH)
- MAJOR: Incompatible API changes
- MINOR: Backwards-compatible functionality additions
- PATCH: Backwards-compatible bug fixes
