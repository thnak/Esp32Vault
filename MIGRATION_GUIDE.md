# Migration Guide: InputManager to Signal Telemetry v1

This guide helps you migrate from the old `InputManager` system to the new **Signal Telemetry v1** system.

## Overview of Changes

### Philosophy Change

**Old System (InputManager)**:
- Firmware made decisions about signals (debouncing, thresholds)
- JSON payloads with interpreted data
- Output control capabilities (trigger, pulse, toggle)

**New System (Signal Telemetry v1)**:
- Firmware = Oscilloscope (captures everything)
- Server = Judge (interprets signals)
- Binary payloads for efficiency
- Focus on input signal capture only

## Breaking Changes

### 1. No More Output Control

The new system **does not support** output control commands:

**Removed**:
- `triggerPin()` - Setting pins HIGH/LOW
- Pulse generation
- Toggle operations

**Why**: Signal Telemetry focuses on accurate signal capture, not control. Use a separate GPIO control system if needed.

### 2. No More Debouncing

**Old**: Debouncing was configurable per pin
```json
{
  "pin": 14,
  "mode": "interrupt",
  "debounce": 50
}
```

**New**: All edges captured, no debouncing
```json
{
  "pin": 14,
  "capture_raw": true
}
```

**Why**: Server decides what's noise and what's signal. Firmware captures everything.

### 3. No More Semantic Modes

**Old**: Multiple modes (input, output, analog, interrupt)

**New**: Only capture modes
- `capture_raw`: Capture all edge changes
- `capture_pulse`: Measure pulse widths

### 4. Binary Payloads Instead of JSON

**Old**: JSON string payloads
```json
{"pin": 14, "value": 1, "timestamp": 12345}
```

**New**: Binary packed structures
```c
struct RawEdge {
    uint8_t  pinId;
    uint8_t  value;
    uint32_t dtUs;
};
```

**Impact**: You need to parse binary data on the server side.

## MQTT Topic Changes

### Old Topics (InputManager)

```
esp32vault/{device_id}/cmd/io/config        - Configure pin
esp32vault/{device_id}/cmd/io/exclude       - Set exclude list
esp32vault/{device_id}/cmd/io/{pin}/trigger - Trigger output
esp32vault/{device_id}/io/{pin}/state       - Pin state (published)
```

### New Topics (Signal Telemetry)

```
raw/{pinId}                                      - Raw edge batches (binary)
pulse/{pinId}                                    - Pulse width (binary)
diag                                             - Diagnostics (binary)
heartbeat                                        - Heartbeat (JSON)
esp32vault/{mac}/cmd/signal/config              - Configure pin
esp32vault/{mac}/cmd/signal/remove              - Remove pin
```

**Note**: Topics now use MAC address instead of device ID, and are simpler (no full path prefix for signal data).

## Configuration Command Changes

### Old: Configure Pin

```json
Topic: esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config
Payload: {
  "pin": 14,
  "mode": "interrupt",
  "edge": "change",
  "debounce": 50,
  "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/14/state",
  "persist": true
}
```

### New: Configure Pin

```json
Topic: esp32vault/A0B1C2D3E4F5/cmd/signal/config
Payload: {
  "pin": 14,
  "capture_raw": true,
  "capture_pulse": true,
  "use_rmt": false
}
```

**Changes**:
- No `mode` - use `capture_raw` and `capture_pulse` flags
- No `edge` - always captures all changes
- No `debounce` - removed
- No `report_topic` - topics are auto-generated as `raw/{pin}` and `pulse/{pin}`
- No `persist` - not implemented yet (future feature)

### Old: Remove Pin

```json
Topic: esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config
Payload: {
  "pin": 14,
  "mode": "none"
}
```

### New: Remove Pin

```json
Topic: esp32vault/A0B1C2D3E4F5/cmd/signal/remove
Payload: {
  "pin": 14
}
```

## Code Migration Examples

### Firmware Side

The firmware has been completely rewritten for ESP-IDF. No Arduino code remains.

### Server Side (Python Example)

**Old**: Parse JSON

```python
import json

def on_message(client, userdata, msg):
    data = json.loads(msg.payload)
    pin = data['pin']
    value = data['value']
    timestamp = data['timestamp']
    print(f"Pin {pin} = {value} at {timestamp}")
```

**New**: Parse Binary

```python
import struct

def on_message(client, userdata, msg):
    if msg.topic.startswith('raw/'):
        pin_id = int(msg.topic.split('/')[1])
        
        # Unpack header
        version, packet_type = struct.unpack('BB', msg.payload[0:2])
        
        # Unpack RawPacket
        base_time_us, base_seq, count = struct.unpack('<QIB', msg.payload[2:15])
        
        # Unpack edges
        offset = 15
        for i in range(count):
            pin, value, dt_us = struct.unpack('<BBI', msg.payload[offset:offset+6])
            abs_time_us = base_time_us + dt_us
            seq = base_seq + i
            print(f"Pin {pin} = {value} at {abs_time_us}us (seq {seq})")
            offset += 6
```

### Server Side (Node.js Example)

**Old**: Parse JSON

```javascript
client.on('message', (topic, payload) => {
  const data = JSON.parse(payload.toString());
  console.log(`Pin ${data.pin} = ${data.value} at ${data.timestamp}`);
});
```

**New**: Parse Binary

```javascript
const struct = require('python-struct');

client.on('message', (topic, payload) => {
  if (topic.startsWith('raw/')) {
    const pinId = parseInt(topic.split('/')[1]);
    
    // Parse header
    const [version, type] = struct.unpack('BB', payload.slice(0, 2));
    
    // Parse RawPacket
    const [baseTimeUs, baseSeq, count] = struct.unpack('<QIB', payload.slice(2, 15));
    
    // Parse edges
    let offset = 15;
    for (let i = 0; i < count; i++) {
      const [pin, value, dtUs] = struct.unpack('<BBI', payload.slice(offset, offset + 6));
      const absTimeUs = baseTimeUs + BigInt(dtUs);
      const seq = baseSeq + i;
      console.log(`Pin ${pin} = ${value} at ${absTimeUs}us (seq ${seq})`);
      offset += 6;
    }
  }
});
```

## Feature Comparison

| Feature                     | InputManager | Signal Telemetry v1 |
|-----------------------------|--------------|---------------------|
| Edge detection              | ✓            | ✓                   |
| Pulse width measurement     | ✗            | ✓ (RMT/ISR)         |
| Debouncing                  | ✓            | ✗                   |
| Output control              | ✓            | ✗                   |
| Analog reading              | ✓            | ✗                   |
| Periodic reporting          | ✓            | ✗                   |
| Batching                    | ✗            | ✓                   |
| Binary payloads             | ✗            | ✓                   |
| Sequence numbers            | ✗            | ✓                   |
| Monotonic timestamps        | ✗            | ✓                   |
| Diagnostic reporting        | ✗            | ✓                   |
| Flood protection            | Basic        | Advanced            |
| ISR-safe architecture       | Yes          | Enhanced            |

## Migration Steps

### 1. Update Server Code

Before upgrading firmware:

1. **Add binary parsing** support for new payload formats
2. **Update topic subscriptions** to new topic structure
3. **Implement debouncing** in server code if needed
4. **Handle sequence numbers** for packet loss detection

### 2. Upgrade Firmware

1. Pull latest code from repository
2. Build and flash using ESP-IDF: `idf.py build && idf.py -p /dev/ttyUSB0 flash`
3. Device will reboot with Signal Telemetry v1

### 3. Reconfigure Pins

Use new configuration commands:

```bash
# Configure a pin for raw edge capture
mosquitto_pub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 14,
    "capture_raw": true,
    "capture_pulse": false
  }'
```

### 4. Monitor and Validate

1. Subscribe to `diag` topic to monitor drops
2. Subscribe to `heartbeat` to confirm device is alive
3. Subscribe to `raw/{pin}` for signal data
4. Check `dropped_raw` and `dropped_pulse` counters

## Backward Compatibility

**None**. This is a breaking change. The new system is incompatible with old InputManager configurations and MQTT topics.

**Recommendation**: Run old and new firmware on separate devices during migration, or maintain separate MQTT topic trees.

## Troubleshooting

### "I need output control!"

Signal Telemetry focuses on signal capture only. For output control:

1. Use the old firmware version, OR
2. Implement a separate GPIO control system, OR
3. Use another ESP32 device dedicated to output control

### "Too many edges are being dropped"

Check diagnostics:

```bash
mosquitto_sub -h broker \
  -t "esp32vault/A0B1C2D3E4F5/status" -v
```

Look at `dropped_raw` counter. If high:

1. Reduce number of pins being monitored
2. Ensure stable network connection
3. Optimize MQTT broker performance
4. Consider filtering on server side after capture

### "I need debouncing!"

Implement debouncing in your server code:

```python
class Debouncer:
    def __init__(self, window_us=50000):  # 50ms default
        self.window_us = window_us
        self.last_time = {}
        self.last_value = {}
    
    def filter(self, pin, value, time_us):
        if pin not in self.last_time:
            self.last_time[pin] = time_us
            self.last_value[pin] = value
            return True
        
        if time_us - self.last_time[pin] < self.window_us:
            return False  # Too soon, filter out
        
        self.last_time[pin] = time_us
        self.last_value[pin] = value
        return True
```

### "Binary payloads are hard to work with"

Use libraries:

- **Python**: `struct` module (built-in)
- **Node.js**: `python-struct` or `ref-struct`
- **Go**: `encoding/binary` package
- **Java**: `ByteBuffer` class
- **C#**: `BinaryReader` class

Or create a small service to convert binary to JSON:

```python
# mqtt_binary_to_json.py
import paho.mqtt.client as mqtt
import struct
import json

def on_message(client, userdata, msg):
    if msg.topic.startswith('raw/'):
        # Parse binary
        # ... (parsing code) ...
        
        # Publish as JSON
        json_data = {
            'pin': pin,
            'value': value,
            'time_us': abs_time_us,
            'seq': seq
        }
        client.publish(f'raw_json/{pin}', json.dumps(json_data))
```

## Benefits of Migration

1. **Higher Accuracy**: Capture every edge without firmware filtering
2. **Better Auditability**: All signal data preserved with sequence numbers
3. **Flexibility**: Server can change logic without firmware updates
4. **Replay Capability**: Reconstruct exact signal timeline from captured data
5. **Better Performance**: Binary payloads are smaller and faster
6. **Professional Grade**: Oscilloscope-level signal capture

## Support

For questions or issues:

1. Check `SIGNAL_TELEMETRY.md` for detailed documentation
2. Review examples in the documentation
3. Check diagnostic messages for system health
4. Open an issue on GitHub repository

## Rollback

If you need to rollback to the previous version:

1. Checkout previous firmware version: `git checkout <previous-commit>`
2. Build and flash with ESP-IDF: `idf.py build && idf.py -p /dev/ttyUSB0 flash`
3. Reconfigure pins using old commands
4. Update server code to expect JSON payloads (if rolling back to pre-Signal Telemetry)

**Note**: Configurations are not preserved between versions.
