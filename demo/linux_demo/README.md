# ESP32 Vault - Linux Demo

This directory contains a demonstration application that runs on Linux host using ESP-IDF's Linux target support.

## Overview

The demo simulates ESP32 Vault's signal telemetry packet serialization without requiring ESP32 hardware. It demonstrates:

- Raw edge change packet creation and serialization
- Pulse width measurement packet creation
- Diagnostic packet creation
- Binary payload format visualization

## Prerequisites

- ESP-IDF v5.4 or later with Linux target support
- Docker (recommended) or native ESP-IDF installation

## Building and Running

### Using Docker (Recommended)

```bash
# Navigate to demo directory
cd demo/linux_demo

# Build demo using ESP-IDF v5.4 container
docker run --rm -v $PWD/../..:/project -w /project/demo/linux_demo espressif/idf:release-v5.4 \
  idf.py set-target linux build

# Run demo
docker run --rm -v $PWD/../..:/project -w /project/demo/linux_demo espressif/idf:release-v5.4 \
  ./build/esp32vault_linux_demo.elf
```

### Using Native ESP-IDF

```bash
# Navigate to demo directory
cd demo/linux_demo

# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Set target to Linux
idf.py set-target linux

# Build demo
idf.py build

# Run demo
./build/esp32vault_linux_demo.elf
```

## What the Demo Shows

The demo creates and displays three types of binary packets:

1. **Raw Edge Packet**: Simulates capturing a square wave signal on pin 14
   - Shows 5 edge changes with timestamps
   - Displays binary payload in hex format

2. **Pulse Width Packet**: Simulates measuring pulse width on pin 15
   - Shows high/low duration measurements
   - Calculates frequency from pulse timing

3. **Diagnostic Packet**: Shows system diagnostic counters
   - Dropped packet counts
   - Queue depth
   - Overflow counters

## Output Example

```
============================================
ESP32 Vault - Linux Demo
Signal Telemetry Packet Demonstration
============================================

=== Raw Edge Packet Demo ===

Packet Info:
  Version: 1
  Type: 1 (raw)
  Base Time: 123456789 us
  Base Seq: 1000
  Edge Count: 5

Edges:
  [0] Pin=14 Value=1 Time=123456789 us (dt=0 us)
  [1] Pin=14 Value=0 Time=123457789 us (dt=1000 us)
  ...

Binary Packet Size: 45 bytes
Binary Payload (hex):
01 01 15 cd 5b 07 00 00 00 00 e8 03 00 00 05 0e
01 00 00 00 00 0e 00 e8 03 00 00 0e 01 d0 07 00
...
```

## Learning from the Demo

This demo helps understand:

- How ESP32 Vault serializes signal data efficiently
- The binary packet format used for MQTT transmission
- Memory layout and alignment of packed structures
- Timestamp encoding and delta compression

## Notes

- All timing uses simulated values
- No actual GPIO or hardware interaction occurs
- Packet structures match exactly with ESP32 firmware
- Binary format is compatible with MQTT parsing tools
