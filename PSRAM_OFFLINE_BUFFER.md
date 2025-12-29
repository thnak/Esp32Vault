# PSRAM Offline Telemetry Buffer

## Overview

The PSRAM offline telemetry buffer is a mandatory feature that ensures no data loss when WiFi/MQTT connectivity is unstable or unavailable. It uses the ESP32's 4MB PSRAM to store telemetry packets that cannot be immediately published to MQTT.

## Architecture

### Design Principles

1. **ISR Safety**: ISR (Interrupt Service Routine) never writes to PSRAM - only to SRAM ring buffer
2. **PSRAM Only for Backlog**: PSRAM is used exclusively for buffering and replay, not for realtime capture
3. **Pre-allocated at Boot**: Buffer is allocated during initialization to prevent fragmentation
4. **Oldest-Drop Policy**: When buffer is full, oldest packets are dropped to make room for new ones
5. **Sequential Replay**: Packets are replayed in sequence number order (ascending)

### Data Flow

```
GPIO ISR → SRAM Ring Buffer → Collect Task → Batch Queue
                                                   ↓
                                            Publish Task
                                                   ↓
                                    ┌──────────────┴─────────────┐
                                    ↓                            ↓
                            MQTT Connected?              MQTT Slow/Down?
                                    ↓                            ↓
                            Direct Publish                 PSRAM Buffer
                                                                 ↓
                                                          Replay Task
                                                        (when reconnected)
```

### MQTT State Detection

| Condition      | Action               | Details                                    |
| -------------- | -------------------- | ------------------------------------------ |
| MQTT connected | Direct publish       | Packets published immediately              |
| MQTT slow      | Spill to PSRAM       | If publish takes >5s, route to PSRAM       |
| MQTT down      | 100% to PSRAM        | All packets buffered in PSRAM              |
| PSRAM full     | Drop oldest + diag++ | Increment drop counter, remove oldest      |

## Implementation

### PSRAMBufferManager Class

The `PSRAMBufferManager` class implements a fixed-size circular buffer pattern:

- **Capacity**: 8,192 packets (~4MB)
- **Structure**: Circular buffer with read/write indices
- **Thread Safety**: Mutex-protected operations
- **Drop Policy**: Oldest packets dropped when full

### Key Methods

```cpp
// Initialize PSRAM buffer (call at boot)
bool begin();

// Add packet to buffer (oldest-drop policy)
bool enqueue(const RawPacket* packet);

// Get next packet for replay
const RawPacket* dequeue();

// Statistics
uint32_t getCount() const;
uint32_t getDroppedCount() const;
float getUsagePercent() const;
```

### Integration with SignalTelemetry

The `SignalTelemetry` class integrates PSRAM buffering:

1. **MQTT State Tracking**: Monitors connection state and publish latency
2. **Publish Task Decision**: Routes packets based on MQTT state
3. **Replay Task**: Background task replays buffered packets when connected

## Configuration

### Enable PSRAM in sdkconfig.defaults

```ini
CONFIG_ESP32_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
```

### Buffer Size Calculation

- Maximum packets: 8,192
- Average packet size: ~500 bytes
- Total buffer size: ~4MB
- Fits comfortably in 4MB PSRAM

## Diagnostics

The device status includes PSRAM buffer statistics:

```json
{
  "psram_buffer_count": 1234,
  "psram_buffer_dropped": 0,
  "psram_buffer_usage_pct": 15.06
}
```

### Diagnostic Fields

- `psram_buffer_count`: Current number of packets in PSRAM buffer
- `psram_buffer_dropped`: Total number of packets dropped due to buffer full
- `psram_buffer_usage_pct`: Buffer usage percentage (0-100)

## Replay Behavior

When MQTT reconnects after being down:

1. **Automatic Detection**: Publish task detects reconnection
2. **Replay Task Start**: Background task created to replay buffered packets
3. **Sequential Order**: Packets replayed in sequence number order (ascending)
4. **No Reordering**: Packets not reordered by pin or any other criteria
5. **No Merging**: Each packet replayed individually, no logic merging
6. **Rate Limiting**: 10ms delay between replays to avoid flooding

## Error Handling

### PSRAM Initialization Failure

If PSRAM is not available or initialization fails:
- System logs critical error
- Continues operation without offline buffering
- Packets dropped when MQTT is unavailable (counted in `dropped_raw`)

### Buffer Full Condition

When PSRAM buffer is full:
1. Oldest packet removed from buffer
2. New packet added
3. Drop counter incremented
4. Warning logged

## Performance Considerations

### Memory Usage

- **PSRAM**: ~4MB for packet buffer
- **SRAM**: Minimal overhead (indices, counters, mutex)
- **Stack**: 4KB for replay task

### CPU Impact

- **Publish Task**: Negligible overhead for state checking
- **Replay Task**: Low priority (priority 2), runs in background
- **ISR**: No impact (ISR doesn't touch PSRAM)

### Network Impact

- Replay rate limited to prevent flooding
- Each packet sent individually with small delay
- Does not interfere with realtime packet publishing

## Limitations

1. **Maximum Buffer**: 8,192 packets (~4MB)
2. **No Persistence**: Buffer cleared on reboot
3. **Sequential Only**: No packet reordering or priority
4. **No Compression**: Packets stored as-is

## Testing

To test the PSRAM offline buffer:

1. **Disconnect MQTT**: Stop broker or block network
2. **Generate Signals**: Trigger GPIO edges to generate packets
3. **Monitor Status**: Check `psram_buffer_count` increasing
4. **Reconnect MQTT**: Start broker or restore network
5. **Verify Replay**: Confirm packets replayed in order

### Expected Behavior

- While disconnected: `psram_buffer_count` increases
- On reconnect: `psram_buffer_count` decreases to 0
- If buffer full: `psram_buffer_dropped` increases
- Sequence numbers preserved in replay

## Future Enhancements

Possible future improvements:

1. **Compression**: Compress packets in PSRAM
2. **Persistence**: Store buffer to flash on reboot
3. **Priority Levels**: Different drop policies per packet type
4. **Adaptive Rate**: Adjust replay rate based on MQTT performance
5. **Statistics History**: Track buffer usage over time
