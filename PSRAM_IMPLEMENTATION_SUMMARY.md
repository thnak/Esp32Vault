# PSRAM Offline Buffering Implementation Summary

## Overview

This document summarizes the implementation of the mandatory PSRAM offline buffering feature for ESP32 Vault's telemetry system. The feature ensures no data loss when WiFi/MQTT connectivity is unstable or unavailable.

## Requirements (from Vietnamese specification)

The original requirement (translated):
- **Mandatory PSRAM offline telemetry**: When WiFi/MQTT has issues, keep records in PSRAM to protect data and ensure SRAM availability
- **PSRAM only for backlog and replay**: Not for realtime capture
- **4MB PSRAM support**
- **Manager pattern**: Fixed-size circular buffer, pre-allocated at boot, oldest-drop policy
- **PSRAM buffer must store enough information to replay**
- **Replay order**: By sequence number (ascending), no pin reordering, no logic merging
- **Errors NOT ALLOWED**: ISR writing to PSRAM, no drop counter

### MQTT Routing Logic

| Condition      | Action               |
| -------------- | -------------------- |
| MQTT connected | Publish normally     |
| MQTT slow      | Spill to PSRAM       |
| MQTT down      | 100% to PSRAM        |
| PSRAM full     | Drop oldest + diag++ |

## Implementation Details

### Files Created

1. **main/PSRAMBufferManager.h**: Header for PSRAM buffer manager
2. **main/PSRAMBufferManager.cpp**: Circular buffer implementation
3. **PSRAM_OFFLINE_BUFFER.md**: Comprehensive documentation

### Files Modified

1. **sdkconfig.defaults**: Added PSRAM configuration
2. **main/SignalTelemetry.h**: Added PSRAM integration
3. **main/SignalTelemetry.cpp**: Updated publish logic and replay mechanism
4. **main/main.cpp**: Added PSRAM diagnostics to status
5. **main/CMakeLists.txt**: Added PSRAMBufferManager.cpp to build
6. **README.md**: Updated with PSRAM features
7. **SIGNAL_TELEMETRY.md**: Updated architecture diagram

## Architecture

### Data Flow

```
GPIO ISR → SRAM Ring Buffer → Collect Task → Batch Queue → Publish Task
                                                                ↓
                                                    ┌───────────┴────────────┐
                                                    ↓                        ↓
                                            MQTT Connected?          MQTT Slow/Down?
                                                    ↓                        ↓
                                            Direct Publish              PSRAM Buffer
                                                                             ↓
                                                                       Replay Task
                                                                             ↓
                                                                    MQTT Publish
```

### Key Components

#### 1. PSRAMBufferManager Class

**Purpose**: Manage circular buffer in PSRAM for offline telemetry storage

**Features**:
- Fixed-size circular buffer (8,192 packets, ~4MB)
- Thread-safe operations with mutex
- Oldest-drop policy when full
- Drop counter tracking
- Usage statistics

**Key Methods**:
```cpp
bool begin();                           // Initialize at boot
bool enqueue(const RawPacket* packet); // Add packet (oldest-drop)
const RawPacket* dequeue();            // Get next packet for replay
uint32_t getCount() const;             // Current buffer depth
uint32_t getDroppedCount() const;      // Total drops
float getUsagePercent() const;         // Usage percentage
```

#### 2. SignalTelemetry Integration

**MQTT State Tracking**:
- `DISCONNECTED`: MQTT not connected
- `CONNECTED_FAST`: MQTT connected and responsive
- `CONNECTED_SLOW`: MQTT connected but slow (>5s publish latency)

**Publish Task Logic**:
```cpp
void publishTaskFunction() {
    while (true) {
        RawPacket batch = waitForBatch();
        updateMQTTState();
        
        if (shouldUseDirectPublish()) {
            if (!tryDirectPublish(&batch)) {
                spillToPSRAM(&batch);
            }
        } else {
            spillToPSRAM(&batch);
        }
    }
}
```

**Replay Task**:
- Created automatically when MQTT reconnects with buffered data
- Priority 2 (lower than publish task)
- Replays packets sequentially
- 10ms delay between replays to avoid flooding
- Self-terminates when buffer empty

#### 3. Diagnostics

New status fields:
```json
{
  "psram_buffer_count": 0,        // Current packets in buffer
  "psram_buffer_dropped": 0,      // Total dropped packets
  "psram_buffer_usage_pct": 0.0   // Buffer usage percentage
}
```

## Requirements Compliance

### ✅ Mandatory Requirements Met

1. **PSRAM offline telemetry**: ✓ Implemented
2. **Manager pattern**: ✓ PSRAMBufferManager class
3. **Fixed-size circular buffer**: ✓ 8,192 packets
4. **Pre-allocate at boot**: ✓ Allocated in begin()
5. **Oldest-drop policy**: ✓ Implemented in enqueue()
6. **4MB PSRAM support**: ✓ Configured in sdkconfig
7. **Drop counter**: ✓ Tracked and exposed in diagnostics
8. **ISR does NOT write PSRAM**: ✓ ISR only writes to SRAM ring buffer
9. **Replay by sequence**: ✓ Sequential dequeue from circular buffer
10. **No pin reordering**: ✓ Replays as stored
11. **No logic merging**: ✓ Each packet replayed individually

### MQTT Routing Logic

| Condition      | Implementation                                    | Status |
| -------------- | ------------------------------------------------- | ------ |
| MQTT connected | Direct publish via tryDirectPublish()             | ✅      |
| MQTT slow      | Detected via >5s latency, spills to PSRAM         | ✅      |
| MQTT down      | !isConnected() → spillToPSRAM()                   | ✅      |
| PSRAM full     | Oldest dropped + droppedPackets++ in enqueue()    | ✅      |

## Memory Usage

### PSRAM (External)
- Buffer: ~4MB (8,192 × 500 bytes avg)
- Allocated once at boot

### SRAM (Internal)
- Minimal: indices (12 bytes), counters (16 bytes), mutex handle (4 bytes)
- Ring buffer: 4KB (unchanged)
- Batch queue: 10 × sizeof(RawPacket) (unchanged)
- Replay task stack: 4KB (when active)

## Performance Impact

### CPU
- Publish task: Minimal overhead for state checking
- Replay task: Low priority (2), doesn't interfere with realtime capture
- ISR: No impact (ISR unchanged)

### Network
- Replay rate: ~100 packets/second (10ms delay)
- Does not block realtime publishing
- Automatic backoff if MQTT becomes slow during replay

## Safety Features

1. **ISR Isolation**: ISR never touches PSRAM, only SRAM ring buffer
2. **Thread Safety**: All PSRAM operations protected by mutex
3. **Graceful Degradation**: If PSRAM unavailable, continues without offline buffering
4. **Memory Protection**: Pre-allocated buffer prevents fragmentation
5. **Overflow Protection**: Oldest-drop policy prevents buffer overflow

## Testing Checklist

To validate the implementation (requires hardware):

- [ ] Boot with PSRAM installed → verify initialization
- [ ] Boot without PSRAM → verify graceful degradation
- [ ] Generate signals while MQTT connected → verify direct publish
- [ ] Disconnect MQTT → verify spillToPSRAM
- [ ] Check status → verify psram_buffer_count increasing
- [ ] Reconnect MQTT → verify replay task starts
- [ ] Check status → verify psram_buffer_count decreasing to 0
- [ ] Fill buffer → verify oldest-drop policy
- [ ] Check status → verify psram_buffer_dropped increments
- [ ] Verify sequence numbers preserved in replay

## Known Limitations

1. **No Persistence**: Buffer cleared on reboot
2. **Fixed Size**: 8,192 packet limit
3. **No Compression**: Packets stored as-is
4. **Sequential Only**: No priority-based replay

## Future Enhancements

Possible improvements:
1. Flash backup for persistence across reboots
2. Compression to increase buffer capacity
3. Adaptive replay rate based on MQTT performance
4. Priority-based drop policy
5. Buffer usage telemetry/alerts

## Conclusion

The PSRAM offline buffering feature has been successfully implemented according to all mandatory requirements. The implementation:

- Ensures no data loss within buffer limits
- Uses PSRAM efficiently for offline storage
- Maintains ISR safety (no PSRAM access)
- Provides automatic replay on reconnection
- Tracks all drops and buffer statistics
- Gracefully handles edge cases

The code is ready for testing on ESP32 hardware with PSRAM.
