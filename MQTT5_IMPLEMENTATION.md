# MQTT5 Implementation Guide

## Overview

ESP32 Vault now fully implements MQTT 5.0 protocol with all the necessary properties for signal telemetry. This document describes the MQTT5 features that have been implemented.

## MQTT5 Features Implemented

### 1. Protocol Version

The MQTT client is configured to use MQTT 5.0:

```cpp
mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_5;
```

### 2. Payload Format Indicator

All messages now include the Payload Format Indicator property:

- **Binary messages** (raw, pulse, diag): `payload_format_indicator = 0`
- **JSON messages** (heartbeat, status): `payload_format_indicator = 1`

### 3. Content-Type Property

Each message type has a specific Content-Type:

```cpp
// Binary telemetry data
#define CONTENT_TYPE_RAW_SIGNAL    "application/vnd.esp32vault.signal.raw+bin"
#define CONTENT_TYPE_PULSE_SIGNAL  "application/vnd.esp32vault.signal.pulse+bin"
#define CONTENT_TYPE_DIAG_SIGNAL   "application/vnd.esp32vault.signal.diag+bin"

// JSON messages
#define CONTENT_TYPE_JSON          "application/json"
```

### 4. Message Expiry Interval

Time-sensitive telemetry data includes a Message Expiry Interval:

- **Raw signal data**: 60 seconds
- **Pulse width data**: 60 seconds
- **Diagnostic data**: No expiry (persistent)
- **Heartbeat/Status**: No expiry (persistent)

### 5. Clean Start

The client uses Clean Start (MQTT5 replacement for Clean Session):

```cpp
mqtt_cfg.session.disable_clean_session = false; // Clean start = true
```

### 6. Keep Alive

Keep alive interval is set to 60 seconds:

```cpp
mqtt_cfg.session.keepalive = 60;
```

## API Usage

### Publishing with MQTT5 Properties

#### Binary Data (Raw/Pulse/Diagnostic)

```cpp
mqttManager->publishBinary(
    "raw/14",                           // Topic
    (const uint8_t*)data,               // Payload
    dataSize,                           // Length
    CONTENT_TYPE_RAW_SIGNAL,           // Content-Type
    false,                              // Retained
    60                                  // Message Expiry (seconds)
);
```

#### JSON Data (Heartbeat/Status)

```cpp
mqttManager->publish(
    "heartbeat",                        // Topic
    jsonPayload,                        // Payload (std::string)
    false                               // Retained
);
```

The JSON publish method automatically sets:
- `payload_format_indicator = 1` (UTF-8)
- `content_type = "application/json"`

## Message Types and Properties

| Message Type | Topic | Content-Type | Format | Expiry |
|-------------|-------|--------------|--------|--------|
| Raw Edges | `raw/{pinId}` | `application/vnd.esp32vault.signal.raw+bin` | Binary | 60s |
| Pulse Width | `pulse/{pinId}` | `application/vnd.esp32vault.signal.pulse+bin` | Binary | 60s |
| Diagnostics | `diag` | `application/vnd.esp32vault.signal.diag+bin` | Binary | None |
| Heartbeat | `heartbeat` | `application/json` | UTF-8 | None |
| Status | `esp32vault/{mac}/status` | `application/json` | UTF-8 | None |

## Benefits of MQTT5 Implementation

### 1. **Proper Content Negotiation**

Clients can now understand the format of messages without guessing:

```python
# Python MQTT client
def on_message(client, userdata, msg):
    content_type = msg.properties.ContentType
    
    if content_type == "application/vnd.esp32vault.signal.raw+bin":
        # Parse as RawPacket structure
        parse_raw_signal(msg.payload)
    elif content_type == "application/json":
        # Parse as JSON
        data = json.loads(msg.payload)
```

### 2. **Message Expiry**

Stale telemetry data automatically expires:

- If a subscriber connects after 60+ seconds, they won't receive old raw/pulse data
- Reduces storage requirements on the broker
- Ensures clients only see recent signal data

### 3. **Format Indicator**

The Payload Format Indicator helps clients:

- Know if the payload is binary (0) or UTF-8 (1)
- Avoid encoding issues
- Process data correctly without trial-and-error

### 4. **Custom Content Types**

Using vendor-specific content types (`application/vnd.esp32vault.*`) provides:

- Clear identification of ESP32 Vault messages
- Version control capability (can add `/v2` later)
- Prevents conflicts with other systems

## Broker Compatibility

This implementation requires an MQTT 5.0 compatible broker:

✅ **Compatible Brokers:**
- Mosquitto 2.0+
- EMQX
- HiveMQ
- AWS IoT Core (with MQTT5 support)
- Azure IoT Hub (with MQTT5 support)

❌ **Incompatible Brokers:**
- Mosquitto 1.x (MQTT 3.1.1 only)
- Older cloud platforms without MQTT5 support

## Configuration

MQTT5 is enabled in `sdkconfig.defaults`:

```
CONFIG_MQTT_PROTOCOL_5=y
```

No runtime configuration is needed - MQTT5 is always used.

## Future Enhancements

Possible future MQTT5 features to add:

1. **User Properties** - Add custom metadata to messages
   ```cpp
   // Example: Add device location
   user_property.key = "location";
   user_property.value = "Lab-A";
   ```

2. **Response Topic** - For command acknowledgments
   ```cpp
   publish_property.response_topic = "esp32vault/{mac}/response";
   ```

3. **Correlation Data** - For request/response patterns
   ```cpp
   publish_property.correlation_data = request_id;
   ```

4. **Topic Alias** - Reduce bandwidth for repeated topics
   ```cpp
   publish_property.topic_alias = 1; // First use: full topic
   // Subsequent uses: empty topic with alias
   ```

## Migration from MQTT 3.1.1

If you're migrating from MQTT 3.1.1:

1. **Update your broker** to support MQTT5
2. **Update client libraries** to MQTT5-compatible versions
3. **Use Content-Type** property to identify message formats
4. **Handle Message Expiry** - old messages may no longer be available

## Testing

To test MQTT5 features:

```bash
# Subscribe with Mosquitto 2.0+
mosquitto_sub -h broker.example.com \
    -t "raw/#" \
    -F "%t: Content-Type=%p{content-type}, Format=%p{payload-format-indicator}" \
    -v

# Publish test message with properties
mosquitto_pub -h broker.example.com \
    -t "test" \
    -m "hello" \
    -D PUBLISH content-type "text/plain" \
    -D PUBLISH payload-format-indicator 1
```

## Compliance

This implementation complies with:

- ✅ MQTT 5.0 Specification (OASIS Standard)
- ✅ Signal Telemetry v1 Specification
- ✅ Binary payload requirements
- ✅ Content-Type best practices (RFC 6838)

## Summary

ESP32 Vault now provides a complete MQTT5 implementation with:

- ✅ Proper content type identification
- ✅ Binary format indicators
- ✅ Message expiry for time-sensitive data
- ✅ Clean start for reliable connections
- ✅ Custom vendor-specific content types

This enables robust, efficient, and standards-compliant signal telemetry over MQTT5.
