# ESP32 Signal Telemetry v1

## Overview

ESP32 Vault has been upgraded to **Signal Telemetry v1**, transforming the firmware into a high-precision signal capture system. The design philosophy is:

> **Firmware = Oscilloscope | Server = Judge**

The firmware captures all signal changes with maximum accuracy and minimal processing, leaving all business logic and filtering to the server.

## Design Principles

1. **Maximum Signal Accuracy** - Capture every edge change without filtering
2. **No Business Logic in Firmware** - Server is the sole authority for signal interpretation
3. **Capture All Available Data** - Don't drop signals unless absolutely necessary
4. **Avoid Network Flooding** - Intelligent batching and prioritization
5. **Auditable & Replayable** - All signals can be reconstructed from captured data

## Architecture

### RTOS Task Structure

```
GPIO ISR / RMT (Hardware)
   ↓
Lock-free Ring Buffer (ISR-safe, SRAM)
   ↓
Signal Collect Task (Priority: 10 - HIGH)
   ↓
Batch Queue
   ↓
MQTT Publish Task (Priority: 3 - LOW)
   ↓
   ├─→ Direct Publish (MQTT connected & fast)
   └─→ PSRAM Buffer (MQTT slow/down)
         ↓
    Replay Task (Priority: 2 - LOWER)
         ↓
    MQTT Publish (when reconnected)
```

### Key Design Decisions

- **ISR does NOT malloc** - Events are copied to pre-allocated ring buffer
- **ISR does NOT publish MQTT** - MQTT runs in separate low-priority task
- **ISR does NOT write PSRAM** - PSRAM only used by publish/replay tasks
- **Queue has hard limits** - Oldest raw edges dropped when full (pulse data preserved when possible)
- **PSRAM Offline Buffer** - 4MB PSRAM buffer stores packets when MQTT unavailable
- **Automatic Replay** - Buffered packets replayed in sequence order when connection restored
- **No debouncing** - All edges captured exactly as they occur
- **No semantic filtering** - Firmware doesn't interpret signal meaning

## MQTT Configuration

### Client Identity

- **Protocol**: MQTT 3.1.1 (targeting MQTT 5 in future)
- **Client ID**: MAC address of ESP32 (e.g., `A0B1C2D3E4F5`)
- **Clean Start**: true
- **Keep Alive**: 60 seconds

### Topic Structure

```
raw/{pinId}        - Raw edge change batches (binary)
pulse/{pinId}      - Pulse width measurements (binary)
diag               - Diagnostic information (binary)
heartbeat          - Device heartbeat (JSON)
```

### MQTT 5 Properties (Future)

For MQTT 5 compatibility, the following properties will be added:

- **Payload Format Indicator**: 0 (binary)
- **Content-Type**: 
  - `application/vnd.esp32vault.signal.raw+bin` for raw edges
  - `application/vnd.esp32vault.signal.pulse+bin` for pulse width
  - `application/vnd.esp32vault.signal.diag+bin` for diagnostics

## Binary Payload Specification

All binary payloads use packed structures with no padding:

### Packet Header

```c
#pragma pack(push, 1)
struct PacketHeader {
    uint8_t version;   // =1
    uint8_t type;      // 1=raw, 2=pulse, 3=diag
};
#pragma pack(pop)
```

### Raw Edge Batch Payload

```c
#pragma pack(push, 1)
struct RawEdge {
    uint8_t  pinId;
    uint8_t  value;      // 0 or 1
    uint32_t dtUs;       // delta from baseTimeUs
};

struct RawPacket {
    PacketHeader header;
    uint64_t baseTimeUs; // absolute time in microseconds (monotonic)
    uint32_t baseSeq;    // sequence number of first event
    uint8_t  count;      // number of edges in this batch
    RawEdge  edges[50];  // variable length, max 50
};
#pragma pack(pop)
```

**Rules**:
- `dtUs` is the delta from `baseTimeUs` in microseconds
- `count` ≤ 50 (MAX_BATCH_SIZE)
- `baseTimeUs` is from `esp_timer_get_time()` (monotonic, resets on reboot)

### Pulse Width Payload

```c
#pragma pack(push, 1)
struct PulsePacket {
    PacketHeader header;
    uint8_t  pinId;
    uint32_t highUs;       // duration of HIGH state in microseconds
    uint32_t lowUs;        // duration of LOW state in microseconds
    uint64_t deviceTimeUs; // absolute time (monotonic)
    uint32_t seq;          // sequence number
};
#pragma pack(pop)
```

### Diagnostic Payload

```c
#pragma pack(push, 1)
struct DiagPacket {
    PacketHeader header;
    uint32_t droppedRaw;    // number of dropped raw edges
    uint32_t droppedPulse;  // number of dropped pulse measurements
    uint16_t queueDepth;    // current batch queue depth
    uint16_t rmtOverflow;   // RMT overflow counter
};
#pragma pack(pop)
```

## Time and Sequence Management

### deviceTimeUs

- **Source**: `esp_timer_get_time()` on ESP32
- **Type**: Monotonic microsecond counter
- **Reset**: On device reboot
- **NOT synchronized**: No UTC/NTP sync
- **Purpose**: Relative timing reconstruction

### seq (Sequence Number)

- **Type**: uint32_t
- **Behavior**: Monotonically increasing
- **Reset**: On device reboot (starts at 0)
- **Purpose**: 
  - Detect packet loss
  - Detect reboot (sequence reset)
  - Detect packet reordering

## Batching and Flood Control

### Batch Strategy

- **Max events per batch**: 50
- **Max time window**: 50ms
- **Partial batches**: Sent when time window expires

### Priority System

| Data Type    | Priority | Drop Policy              |
|--------------|----------|--------------------------|
| Pulse Width  | HIGH     | Never drop if possible   |
| Raw Edge     | MEDIUM   | Drop oldest when full    |
| Heartbeat    | LOW      | Periodic only            |

### Queue Full Policy

When the batch queue is full:
1. Remove oldest raw edge batch
2. Increment `droppedRaw` counter by batch size
3. Insert new batch
4. Try to preserve pulse data

## Signal Capture Requirements

### Raw Level Change (MANDATORY)

**What is captured**:
- Every GPIO level change (rising and falling edges)
- Pin ID
- Value (0 or 1)
- Timestamp in microseconds
- Sequence number

**What is NOT done**:
- ❌ No debouncing
- ❌ No filtering
- ❌ No threshold logic
- ❌ No semantic interpretation

### Pulse Width Measurement (STRONGLY RECOMMENDED)

**Preferred Method**: RMT (Remote Control) RX peripheral

**Fallback Method**: ISR + `esp_timer_get_time()`

**Captured Data**:
- Duration of HIGH state (`highUs`)
- Duration of LOW state (`lowUs`)
- Timestamp
- Sequence number

## Configuration Commands

### Configure Pin for Signal Capture

```json
Topic: esp32vault/{mac_address}/cmd/signal/config
Payload: {
  "pin": 14,
  "capture_raw": true,
  "capture_pulse": true,
  "use_rmt": true
}
```

**Parameters**:
- `pin`: GPIO pin number (0-39 on ESP32)
- `capture_raw`: Capture all edge changes (default: true)
- `capture_pulse`: Measure pulse widths (default: false)
- `use_rmt`: Use RMT peripheral for pulse measurement (default: false)

### Remove Pin Configuration

```json
Topic: esp32vault/{mac_address}/cmd/signal/remove
Payload: {
  "pin": 14
}
```

## Boot/Reboot Behavior

On device boot, the firmware:

1. Resets `seq` to 0
2. Resets `deviceTimeUs` (automatic with `esp_timer_get_time()`)
3. Publishes heartbeat message
4. Publishes initial diagnostic snapshot

Server can detect reboot by:
- Sequence number reset to low values
- Heartbeat message after long silence
- `deviceTimeUs` values lower than previous

## Firmware Guarantees

The firmware guarantees:

✅ **No missed edges** within hardware capability
✅ **Accurate `deviceTimeUs`** from hardware timer
✅ **Deterministic payloads** - same input = same output
✅ **Replayable & auditable** - all data preserved
✅ **No network flooding** - intelligent batching

## What the Firmware Does NOT Do

❌ **No debouncing** - every edge is captured
❌ **No threshold logic** - no voltage level interpretation
❌ **No semantic filtering** - no "button press" vs "noise" distinction
❌ **No string payloads** - binary only for efficiency
❌ **No reliance on network timing** - uses local monotonic time

## Monitoring and Diagnostics

### Heartbeat Messages

Published every 30 seconds to `heartbeat` topic:

```json
{
  "mac": "A0B1C2D3E4F5",
  "seq": 12345,
  "uptime": 3600
}
```

### Diagnostic Messages

Published every 60 seconds to `diag` topic (binary):

- Dropped raw edges count
- Dropped pulse measurements count
- Current queue depth
- RMT overflow events

### Device Status

Published every 30 seconds to `esp32vault/{mac}/status`:

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
  "queue_depth": 2
}
```

## Example Usage

### 1. Configure a pin to capture all edges

```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 14,
    "capture_raw": true,
    "capture_pulse": false,
    "use_rmt": false
  }'
```

### 2. Configure a pin for pulse width measurement with RMT

```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 27,
    "capture_raw": true,
    "capture_pulse": true,
    "use_rmt": true
  }'
```

### 3. Subscribe to raw edge data (binary)

```bash
mosquitto_sub -h broker.example.com \
  -t "raw/14" \
  -F "%t: %x" -v
```

### 4. Subscribe to pulse width data (binary)

```bash
mosquitto_sub -h broker.example.com \
  -t "pulse/27" \
  -F "%t: %x" -v
```

### 5. Monitor diagnostics

```bash
mosquitto_sub -h broker.example.com \
  -t "diag" \
  -F "%t: %x" -v
```

## Performance Characteristics

### Timing Accuracy

- **ISR latency**: < 5 microseconds (typical)
- **Timestamp resolution**: 1 microsecond
- **Batch publish latency**: < 50ms typical

### Memory Usage

- **Ring buffer**: 4KB
- **Batch queue**: ~2KB (10 batches × ~200 bytes)
- **Total overhead**: ~6KB additional RAM

### Throughput

- **Max edges/second**: ~10,000 (with batching)
- **Max batch rate**: 20 batches/second
- **MQTT payload size**: Typically 50-500 bytes per batch

## Migration from Old InputManager

The old `InputManager` has been replaced with `SignalTelemetry`. Key differences:

| Feature              | Old InputManager        | New SignalTelemetry     |
|----------------------|-------------------------|-------------------------|
| Debouncing           | Yes (configurable)      | **No** (removed)        |
| Output control       | Yes (trigger commands)  | **No** (removed)        |
| Semantic filtering   | Yes (threshold, etc)    | **No** (removed)        |
| Payload format       | JSON strings            | **Binary packets**      |
| Batching             | No                      | **Yes** (required)      |
| Pulse measurement    | No                      | **Yes** (RMT/ISR)       |
| Priority queue       | No                      | **Yes** (multi-level)   |

## Future Enhancements

1. **MQTT 5 Support** - Full MQTT 5 with properties
2. **TLS/SSL** - Secure MQTT connections
3. **Multi-pin Pulse** - Correlate pulses across pins
4. **Hardware Timestamps** - GPIO interrupt hardware timestamps
5. **Compression** - Optional payload compression for low bandwidth

## Troubleshooting

### High droppedRaw counter

**Cause**: Too many edges for current batch/publish rate

**Solutions**:
- Reduce number of configured pins
- Ensure stable network connection
- Check MQTT broker performance

### RMT configuration failures

**Cause**: Limited RMT channels (8 on ESP32)

**Solution**: Use ISR fallback (set `use_rmt: false`)

### No data on raw/ topics

**Check**:
1. Pin is configured: Check status message
2. Pin has activity: Test with oscilloscope or LED
3. MQTT connected: Check device status
4. Subscribed to correct topic: `raw/{pinId}`

## Compliance with Specification

This implementation follows the "ESP32 Firmware Checksheet – Signal Telemetry v1" specification:

- ✅ MQTT client ID = MAC address
- ✅ Binary payloads with proper structures
- ✅ RTOS architecture (ISR → Ring Buffer → Tasks)
- ✅ Raw level change capture (no filtering)
- ✅ Pulse width measurement (RMT preferred)
- ✅ Batching with flood control
- ✅ Topic convention (raw/, pulse/, diag, heartbeat)
- ✅ Diagnostic reporting
- ✅ Boot/reboot behavior (seq reset, heartbeat, diag)
- ✅ No debounce, no threshold, no semantic logic
- ✅ Oscilloscope philosophy

## Summary

ESP32 Vault Signal Telemetry v1 transforms your ESP32 into a precise signal capture device, forwarding all signal data to your server for processing. This approach provides maximum flexibility, auditability, and replay capability for IoT signal monitoring applications.
