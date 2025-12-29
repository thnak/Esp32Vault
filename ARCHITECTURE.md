# ESP32 Vault Architecture - Signal Telemetry v1

Complete architecture documentation is now available in:

- **[IMPLEMENTATION_COMPLETE.md](IMPLEMENTATION_COMPLETE.md)** - Full implementation summary with architecture details
- **[SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md)** - Signal capture system specification
- **[ESP_IDF_BUILD.md](ESP_IDF_BUILD.md)** - ESP-IDF build system and components

## Quick Architecture Overview

### System Components

1. **WiFiManager** (`main/WiFiManager.cpp`) - ESP-IDF WiFi management
2. **MQTTManager** (`main/MQTTManager.cpp`) - MQTT5 with esp-mqtt
3. **OTAManager** (`main/OTAManager.cpp`) - HTTP/HTTPS OTA updates
4. **SignalTelemetry** (`main/SignalTelemetry.cpp`) - Core signal capture system

### RTOS Signal Path

```
GPIO ISR → Ring Buffer → Collect Task (high pri) → Batch Queue → Publish Task (low pri) → MQTT
```

### Design Philosophy

> **Firmware = Oscilloscope | Server = Judge**

- Firmware captures ALL signal changes without filtering
- Microsecond precision timestamps
- Binary payloads for efficiency
- Server applies all business logic

### Binary Protocol

- **raw/{pinId}** - Edge batches (6 bytes/edge)
- **pulse/{pinId}** - Pulse widths (23 bytes)
- **diag** - Diagnostics (14 bytes)
- **heartbeat** - Device status (JSON)

### Performance

- ISR latency: < 5μs
- Timestamp resolution: 1μs
- Throughput: ~10,000 edges/second
- Memory: ~6KB additional RAM

See linked documents above for complete details.
