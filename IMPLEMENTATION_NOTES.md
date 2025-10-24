# IO Management Implementation Notes

## Overview

This document provides technical details about the Dynamic IO Management implementation for ESP32 Vault v1.1.0.

## Architecture

### Component: InputManager

The InputManager is a new component that manages GPIO pins dynamically through MQTT commands. It uses FreeRTOS for task management and ISR-safe operations.

```
┌─────────────────────────────────────────────────────────┐
│                    InputManager                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Configuration Storage (NVS)                            │
│  ├── Pin Configurations (namespace: "io")              │
│  └── Exclusion List (namespace: "io")                  │
│                                                         │
│  Runtime Components                                     │
│  ├── Event Queue (FreeRTOS)                            │
│  │   ├── Size: 32 events                               │
│  │   ├── Strategy: Overwrite oldest when full          │
│  │   └── Type: ISR-safe (xQueueSendFromISR)            │
│  │                                                      │
│  ├── Worker Task (FreeRTOS)                            │
│  │   ├── Stack: 4096 bytes                             │
│  │   ├── Priority: 5                                   │
│  │   └── Functions: Event processing, debouncing       │
│  │                                                      │
│  └── ISR Handlers                                       │
│      ├── IRAM_ATTR for fast response                   │
│      ├── Minimal processing in ISR                     │
│      └── Queue events for worker task                  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Key Design Decisions

### 1. Enum Naming Convention

**Problem**: Arduino framework defines macros like `OUTPUT`, `INPUT`, `RISING`, `FALLING`, `CHANGE`, and `ANALOG` which conflict with enum names.

**Solution**: Added suffixes to avoid conflicts:
- `OUTPUT` → `OUTPUT_MODE`
- `INPUT` → `INPUT_MODE`
- `RISING` → `RISING_EDGE`
- `FALLING` → `FALLING_EDGE`
- `CHANGE` → `CHANGE_EDGE`
- `ANALOG` → `ANALOG_MODE` or `ANALOG_READ`

### 2. ISR-Safe Event Queue

**Design**: Use FreeRTOS queue for communication between ISR and worker task.

**Implementation**:
```cpp
// Queue creation
eventQueue = xQueueCreate(QUEUE_SIZE, sizeof(IOEvent));

// ISR handler
void IRAM_ATTR handleInterrupt(void* arg) {
    // If queue full, remove oldest
    if (uxQueueSpacesAvailable(eventQueue) == 0) {
        IOEvent dummy;
        xQueueReceiveFromISR(eventQueue, &dummy, &xHigherPriorityTaskWoken);
    }
    xQueueSendFromISR(eventQueue, &event, &xHigherPriorityTaskWoken);
}
```

**Rationale**: ISR must be minimal and fast. Queue allows safe communication with worker task for heavy processing.

### 3. Overwrite-Oldest Strategy

**Problem**: What to do when event queue is full?

**Options Considered**:
1. Drop new events
2. Drop oldest events
3. Increase queue size

**Choice**: Drop oldest events (overwrite-oldest)

**Rationale**: In high-frequency interrupt scenarios, recent events are more relevant than old unprocessed events. This ensures the system always reflects the current state.

### 4. Pin Validation

**Layers of Validation**:
1. **Reserved Pins**: Hardcoded list (SPI flash pins: 6-11)
2. **Excluded Pins**: User-configurable via MQTT
3. **Pin Number Range**: Implicit validation by uint8_t type

**Flow**:
```
Pin Configuration Request
    ↓
Is pin reserved? → YES → Reject
    ↓ NO
Is pin excluded? → YES → Reject
    ↓ NO
Configure pin → Success
```

### 5. Mandatory report_topic

**Decision**: Every configured pin MUST have a report_topic.

**Rationale**: 
- Ensures visibility of pin states
- Facilitates debugging and monitoring
- Enables remote monitoring without polling
- Consistent with MQTT pub/sub pattern

### 6. Debounce Implementation

**Approach**: Software debounce in worker task, not in ISR.

**Implementation**:
```cpp
if (config.debounceMs > 0) {
    if (now - config.lastReportTime < config.debounceMs) {
        return; // Too soon, ignore
    }
}
```

**Rationale**: Keeps ISR fast, allows flexible debounce periods per pin.

## Memory Management

### Static Allocations

- **Event Queue**: 32 × sizeof(IOEvent) = 32 × 16 = 512 bytes
- **Worker Task Stack**: 4096 bytes
- **Pin Configuration Map**: Dynamic, ~100 bytes per pin
- **ISR Handler Map**: Dynamic, ~20 bytes per interrupt pin

### Dynamic Allocations

- **String objects**: For topics, mode names, etc.
- **JSON documents**: StaticJsonDocument used with fixed sizes
- **STL containers**: std::map and std::vector with reasonable limits

### Total Impact

- **Flash**: +35.6 KB (code + strings)
- **RAM**: +776 bytes static + dynamic per pin configuration

## Persistence Strategy

### What is Persisted

1. **Pin Configurations** (if persist: true)
   - Pin number
   - Mode
   - Edge detection
   - Debounce time
   - Pulse width
   - Report interval
   - Report topic
   - Retain flag

2. **Exclusion List** (if persist: true)
   - Individual pins
   - Pin ranges

### Storage Format

JSON format in NVS:
```json
{
  "pins": [
    {
      "pin": 13,
      "mode": "output",
      "report_topic": "esp32vault/xxx/io/13/state",
      "persist": true
    }
  ]
}
```

### Recovery on Boot

1. Load exclusion list from NVS
2. Load pin configurations from NVS
3. Validate each pin against exclusion list
4. Configure hardware for each valid pin
5. Attach interrupts if needed

## Error Handling

### Configuration Errors

- **Invalid pin number**: Reject with error message
- **Reserved/excluded pin**: Reject with error message
- **Invalid mode**: Reject with error message
- **Missing report_topic**: Reject with error message
- **Malformed JSON**: Ignore and log error

### Runtime Errors

- **Queue full**: Overwrite oldest (by design)
- **ISR handler not found**: Silent fail (defensive)
- **MQTT disconnected**: Queue events, publish when reconnected
- **NVS write failure**: Log error, continue operation

## Performance Characteristics

### Latency

- **Interrupt to Queue**: < 100μs (ISR)
- **Queue to Processing**: < 10ms (task scheduling)
- **Processing to MQTT**: Variable (network dependent)
- **Total latency**: Typically < 50ms

### Throughput

- **Maximum event rate**: ~100 events/second (limited by debounce)
- **MQTT publish rate**: Depends on broker and network
- **Queue capacity**: 32 events

### CPU Usage

- **Idle**: ~0% (task blocked on queue)
- **Processing events**: < 5% (per event)
- **MQTT publishing**: < 2% (per message)

## Security Considerations

### Input Validation

✅ Pin numbers validated against reserved/excluded lists
✅ JSON parsing with error checking
✅ Bounds checking on all numeric inputs
✅ String length limits enforced by JSON document sizes

### Memory Safety

✅ No unsafe string functions (strcpy, sprintf)
✅ StaticJsonDocument with fixed sizes
✅ STL containers with implicit bounds checking
✅ FreeRTOS queue prevents buffer overflows

### Access Control

⚠️ **Limitation**: No authentication on MQTT topics
- Recommendation: Use MQTT broker ACLs
- Recommendation: Enable TLS for production
- Recommendation: Use strong broker authentication

## Testing Recommendations

### Unit Tests (if implementing)

1. Pin validation logic
2. Event queue overflow handling
3. Debounce timing
4. JSON parsing edge cases
5. NVS persistence and recovery

### Integration Tests

1. Configure pin via MQTT → verify hardware state
2. Trigger interrupt → verify MQTT publication
3. Fill event queue → verify oldest overwritten
4. Persist config → restart → verify config loaded
5. Exclude pin → attempt config → verify rejected

### Stress Tests

1. Rapid interrupt generation
2. Many simultaneous pin configurations
3. Queue overflow scenarios
4. Long-running stability test
5. Memory leak detection

## Known Limitations

1. **Maximum pins**: Limited by available GPIO and RAM
2. **Event rate**: Debounce limits effective event rate
3. **MQTT dependency**: No local control without MQTT
4. **Queue size**: Fixed at 32 events
5. **No priority**: All events treated equally
6. **Network latency**: MQTT adds variable latency

## Future Enhancements

### Possible Improvements

1. **Priority queue**: Critical events processed first
2. **Local triggers**: GPIO-to-GPIO mapping without MQTT
3. **Batch operations**: Configure multiple pins atomically
4. **PWM support**: Analog output with duty cycle control
5. **Pulse counting**: Count pulses without reporting each
6. **Event filtering**: Reduce MQTT traffic with smart filtering
7. **Watchdog**: Automatic recovery from hung worker task

### Breaking Changes to Consider

1. Change queue to ring buffer for better performance
2. Add authentication to IO commands
3. Switch to binary protocol for efficiency
4. Add versioning to stored configurations

## Migration Guide

### From v1.0.0 to v1.1.0

**No breaking changes**: v1.1.0 is fully backward compatible.

**New features available immediately**:
- IO management commands work out of the box
- No configuration changes needed
- Existing MQTT topics unchanged

**To use IO management**:
1. Ensure MQTT is configured
2. Send configuration commands to `/cmd/io/config`
3. Subscribe to `/io/{pin}/state` for state updates

## Troubleshooting

### Common Issues

**Issue**: "Pin X is excluded or reserved"
- **Cause**: Pin in exclusion list or default reserved list
- **Solution**: Choose different pin or modify exclusion list

**Issue**: "report_topic is required"
- **Cause**: Missing report_topic in configuration
- **Solution**: Add report_topic to configuration JSON

**Issue**: No state reports received
- **Cause**: MQTT disconnected or topic misconfigured
- **Solution**: Check MQTT connection and topic spelling

**Issue**: Interrupt not triggering
- **Cause**: Wrong edge detection or hardware issue
- **Solution**: Try "change" edge detection, verify wiring

**Issue**: Queue overflow warnings
- **Cause**: Event rate exceeds processing capacity
- **Solution**: Increase debounce time or reduce event sources

## References

### Documentation

- [ESP32 GPIO Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- [FreeRTOS Queue Documentation](https://www.freertos.org/a00018.html)
- [Arduino ESP32 Reference](https://github.com/espressif/arduino-esp32)

### Related Files

- `include/InputManager.h` - Header file
- `src/InputManager.cpp` - Implementation
- `IO_USAGE_EXAMPLES.md` - Usage examples
- `example_mqtt_commands.md` - MQTT command reference

## Contact

For issues, questions, or contributions, please use the GitHub repository issue tracker.
