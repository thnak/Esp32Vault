/*
 * ESP32 Vault - Signal Telemetry Unit Tests
 * 
 * Tests for signal telemetry binary packet structures and serialization.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "unity.h"

// Binary packet structures (copied from SignalTelemetry.h for testing)
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

void test_signal_telemetry_packet_serialization(void)
{
    // Test that packet structures have correct sizes for binary serialization
    
    // Header should be 2 bytes
    TEST_ASSERT_EQUAL_UINT(2, sizeof(struct PacketHeader));
    
    // RawEdge should be 6 bytes (1 + 1 + 4)
    TEST_ASSERT_EQUAL_UINT(6, sizeof(struct RawEdge));
    
    // RawPacket header fields: 2 + 8 + 4 + 1 = 15 bytes + edges
    TEST_ASSERT_EQUAL_UINT(15 + (50 * 6), sizeof(struct RawPacket));
    
    // PulsePacket: 2 + 1 + 4 + 4 + 8 + 4 = 23 bytes
    TEST_ASSERT_EQUAL_UINT(23, sizeof(struct PulsePacket));
    
    // DiagPacket: 2 + 4 + 4 + 2 + 2 = 14 bytes
    TEST_ASSERT_EQUAL_UINT(14, sizeof(struct DiagPacket));
}

void test_raw_packet_structure(void)
{
    struct RawPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 1; // raw
    
    // Set packet data
    packet.baseTimeUs = 1000000ULL;
    packet.baseSeq = 100;
    packet.count = 3;
    
    // Add edges
    packet.edges[0].pinId = 14;
    packet.edges[0].value = 1;
    packet.edges[0].dtUs = 0;
    
    packet.edges[1].pinId = 14;
    packet.edges[1].value = 0;
    packet.edges[1].dtUs = 500;
    
    packet.edges[2].pinId = 14;
    packet.edges[2].value = 1;
    packet.edges[2].dtUs = 1000;
    
    // Verify structure
    TEST_ASSERT_EQUAL_UINT8(1, packet.header.version);
    TEST_ASSERT_EQUAL_UINT8(1, packet.header.type);
    TEST_ASSERT_EQUAL_UINT64(1000000ULL, packet.baseTimeUs);
    TEST_ASSERT_EQUAL_UINT32(100, packet.baseSeq);
    TEST_ASSERT_EQUAL_UINT8(3, packet.count);
    
    // Verify edges
    TEST_ASSERT_EQUAL_UINT8(14, packet.edges[0].pinId);
    TEST_ASSERT_EQUAL_UINT8(1, packet.edges[0].value);
    TEST_ASSERT_EQUAL_UINT32(0, packet.edges[0].dtUs);
    
    TEST_ASSERT_EQUAL_UINT8(14, packet.edges[1].pinId);
    TEST_ASSERT_EQUAL_UINT8(0, packet.edges[1].value);
    TEST_ASSERT_EQUAL_UINT32(500, packet.edges[1].dtUs);
    
    TEST_ASSERT_EQUAL_UINT8(14, packet.edges[2].pinId);
    TEST_ASSERT_EQUAL_UINT8(1, packet.edges[2].value);
    TEST_ASSERT_EQUAL_UINT32(1000, packet.edges[2].dtUs);
}

void test_pulse_packet_structure(void)
{
    struct PulsePacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 2; // pulse
    
    // Set packet data
    packet.pinId = 15;
    packet.highUs = 1000;
    packet.lowUs = 2000;
    packet.deviceTimeUs = 5000000ULL;
    packet.seq = 200;
    
    // Verify structure
    TEST_ASSERT_EQUAL_UINT8(1, packet.header.version);
    TEST_ASSERT_EQUAL_UINT8(2, packet.header.type);
    TEST_ASSERT_EQUAL_UINT8(15, packet.pinId);
    TEST_ASSERT_EQUAL_UINT32(1000, packet.highUs);
    TEST_ASSERT_EQUAL_UINT32(2000, packet.lowUs);
    TEST_ASSERT_EQUAL_UINT64(5000000ULL, packet.deviceTimeUs);
    TEST_ASSERT_EQUAL_UINT32(200, packet.seq);
}

void test_diag_packet_structure(void)
{
    struct DiagPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Set header
    packet.header.version = 1;
    packet.header.type = 3; // diag
    
    // Set packet data
    packet.droppedRaw = 10;
    packet.droppedPulse = 5;
    packet.queueDepth = 3;
    packet.rmtOverflow = 2;
    
    // Verify structure
    TEST_ASSERT_EQUAL_UINT8(1, packet.header.version);
    TEST_ASSERT_EQUAL_UINT8(3, packet.header.type);
    TEST_ASSERT_EQUAL_UINT32(10, packet.droppedRaw);
    TEST_ASSERT_EQUAL_UINT32(5, packet.droppedPulse);
    TEST_ASSERT_EQUAL_UINT16(3, packet.queueDepth);
    TEST_ASSERT_EQUAL_UINT16(2, packet.rmtOverflow);
}
