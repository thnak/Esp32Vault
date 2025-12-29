/*
 * ESP32 Vault - Linux Demo Application
 * 
 * This demo simulates signal capture and telemetry on Linux host.
 * It demonstrates packet serialization without requiring ESP32 hardware.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

// Binary packet structures (same as SignalTelemetry)
// NOTE: These are intentionally duplicated here for the demo to be self-contained
// and demonstrate the exact binary format used by ESP32 Vault. Any changes to
// SignalTelemetry.h structures should be reflected here.
#pragma pack(push, 1)

struct PacketHeader {
    uint8_t version;   // =1
    uint8_t type;      // 1=raw, 2=pulse, 3=diag
};

struct RawEdge {
    uint8_t  pinId;
    uint8_t  value;
    uint32_t dtUs;     // delta from baseTimeUs
};

struct RawPacket {
    struct PacketHeader header;
    uint64_t baseTimeUs;
    uint32_t baseSeq;
    uint8_t  count;
    struct RawEdge edges[50]; // MAX_BATCH
};

struct PulsePacket {
    struct PacketHeader header;
    uint8_t  pinId;
    uint32_t highUs;
    uint32_t lowUs;
    uint64_t deviceTimeUs;
    uint32_t seq;
};

struct DiagPacket {
    struct PacketHeader header;
    uint32_t droppedRaw;
    uint32_t droppedPulse;
    uint16_t queueDepth;
    uint16_t rmtOverflow;
};

#pragma pack(pop)

// Get current time in microseconds
uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

// Print packet in hex format
void print_hex(const uint8_t* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

// Demonstrate raw edge packet
void demo_raw_packet(void)
{
    printf("\n=== Raw Edge Packet Demo ===\n");
    
    struct RawPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 1; // raw
    
    // Simulate signal capture
    packet.baseTimeUs = get_time_us();
    packet.baseSeq = 1000;
    packet.count = 5;
    
    // Simulate a square wave on pin 14
    packet.edges[0].pinId = 14;
    packet.edges[0].value = 1;
    packet.edges[0].dtUs = 0;
    
    packet.edges[1].pinId = 14;
    packet.edges[1].value = 0;
    packet.edges[1].dtUs = 1000; // 1ms high
    
    packet.edges[2].pinId = 14;
    packet.edges[2].value = 1;
    packet.edges[2].dtUs = 2000; // 1ms low
    
    packet.edges[3].pinId = 14;
    packet.edges[3].value = 0;
    packet.edges[3].dtUs = 3000; // 1ms high
    
    packet.edges[4].pinId = 14;
    packet.edges[4].value = 1;
    packet.edges[4].dtUs = 4000; // 1ms low
    
    printf("\nPacket Info:\n");
    printf("  Version: %d\n", packet.header.version);
    printf("  Type: %d (raw)\n", packet.header.type);
    printf("  Base Time: %llu us\n", (unsigned long long)packet.baseTimeUs);
    printf("  Base Seq: %u\n", packet.baseSeq);
    printf("  Edge Count: %u\n", packet.count);
    
    printf("\nEdges:\n");
    for (int i = 0; i < packet.count; i++) {
        printf("  [%d] Pin=%u Value=%u Time=%llu us (dt=%u us)\n",
               i,
               packet.edges[i].pinId,
               packet.edges[i].value,
               (unsigned long long)(packet.baseTimeUs + packet.edges[i].dtUs),
               packet.edges[i].dtUs);
    }
    
    // Calculate packet size for transmission
    size_t packet_size = 2 + 8 + 4 + 1 + (packet.count * 6);
    printf("\nBinary Packet Size: %zu bytes\n", packet_size);
    printf("\nBinary Payload (hex):\n");
    print_hex((const uint8_t*)&packet, packet_size);
}

// Demonstrate pulse width packet
void demo_pulse_packet(void)
{
    printf("\n=== Pulse Width Packet Demo ===\n");
    
    struct PulsePacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 2; // pulse
    
    // Simulate pulse measurement
    packet.pinId = 15;
    packet.highUs = 5000;   // 5ms high
    packet.lowUs = 10000;   // 10ms low
    packet.deviceTimeUs = get_time_us();
    packet.seq = 2000;
    
    printf("\nPacket Info:\n");
    printf("  Version: %d\n", packet.header.version);
    printf("  Type: %d (pulse)\n", packet.header.type);
    printf("  Pin: %u\n", packet.pinId);
    printf("  High Duration: %u us (%.3f ms)\n", packet.highUs, packet.highUs / 1000.0);
    printf("  Low Duration: %u us (%.3f ms)\n", packet.lowUs, packet.lowUs / 1000.0);
    printf("  Period: %u us (%.3f ms, %.1f Hz)\n",
           packet.highUs + packet.lowUs,
           (packet.highUs + packet.lowUs) / 1000.0,
           1000000.0 / (packet.highUs + packet.lowUs));
    printf("  Device Time: %llu us\n", (unsigned long long)packet.deviceTimeUs);
    printf("  Sequence: %u\n", packet.seq);
    
    size_t packet_size = sizeof(struct PulsePacket);
    printf("\nBinary Packet Size: %zu bytes\n", packet_size);
    printf("\nBinary Payload (hex):\n");
    print_hex((const uint8_t*)&packet, packet_size);
}

// Demonstrate diagnostic packet
void demo_diag_packet(void)
{
    printf("\n=== Diagnostic Packet Demo ===\n");
    
    struct DiagPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 3; // diag
    
    // Simulate diagnostic data
    packet.droppedRaw = 42;
    packet.droppedPulse = 5;
    packet.queueDepth = 8;
    packet.rmtOverflow = 2;
    
    printf("\nPacket Info:\n");
    printf("  Version: %d\n", packet.header.version);
    printf("  Type: %d (diagnostic)\n", packet.header.type);
    printf("  Dropped Raw: %u\n", packet.droppedRaw);
    printf("  Dropped Pulse: %u\n", packet.droppedPulse);
    printf("  Queue Depth: %u\n", packet.queueDepth);
    printf("  RMT Overflow: %u\n", packet.rmtOverflow);
    
    size_t packet_size = sizeof(struct DiagPacket);
    printf("\nBinary Packet Size: %zu bytes\n", packet_size);
    printf("\nBinary Payload (hex):\n");
    print_hex((const uint8_t*)&packet, packet_size);
}

void app_main(void)
{
    printf("\n");
    printf("============================================\n");
    printf("ESP32 Vault - Linux Demo\n");
    printf("Signal Telemetry Packet Demonstration\n");
    printf("============================================\n");
    printf("\n");
    printf("This demo shows how ESP32 Vault serializes\n");
    printf("signal telemetry data into binary packets.\n");
    printf("\n");
    
    // Demonstrate each packet type
    demo_raw_packet();
    sleep(1);
    
    demo_pulse_packet();
    sleep(1);
    
    demo_diag_packet();
    
    printf("\n");
    printf("============================================\n");
    printf("Demo Complete!\n");
    printf("============================================\n");
    printf("\n");
    printf("Note: In real ESP32 operation, these binary\n");
    printf("packets are published to MQTT topics:\n");
    printf("  - raw/{pin}   : Raw edge packets\n");
    printf("  - pulse/{pin} : Pulse width packets\n");
    printf("  - diag        : Diagnostic packets\n");
    printf("\n");
}
