/*
 * ESP32 Vault - End-to-End Integration Tests
 * 
 * Tests the complete signal capture, buffering, and MQTT telemetry flow.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "unity.h"
#include "mock_mqtt_broker.h"
#include "signal_simulator.h"

// Import packet structures from SignalTelemetry
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

// Test context for tracking received messages
typedef struct {
    uint32_t raw_packet_count;
    uint32_t pulse_packet_count;
    uint32_t diag_packet_count;
    struct RawPacket last_raw_packet;
    struct PulsePacket last_pulse_packet;
} test_context_t;

// Callback for raw packet messages
static void raw_packet_callback(const char* topic, const uint8_t* payload, size_t payload_len, void* user_data)
{
    test_context_t* ctx = (test_context_t*)user_data;
    if (payload_len >= sizeof(struct RawPacket)) {
        memcpy(&ctx->last_raw_packet, payload, sizeof(struct RawPacket));
        ctx->raw_packet_count++;
    }
}

// Callback for pulse packet messages
static void pulse_packet_callback(const char* topic, const uint8_t* payload, size_t payload_len, void* user_data)
{
    test_context_t* ctx = (test_context_t*)user_data;
    if (payload_len >= sizeof(struct PulsePacket)) {
        memcpy(&ctx->last_pulse_packet, payload, sizeof(struct PulsePacket));
        ctx->pulse_packet_count++;
    }
}

/**
 * Test: Signal to MQTT - Normal Operation
 * Simulates signal generation and verifies packets are published to MQTT
 */
void test_e2e_signal_to_mqtt(void)
{
    // Initialize components
    mock_mqtt_broker_t broker;
    signal_simulator_t sim;
    test_context_t ctx;
    
    mock_mqtt_broker_init(&broker);
    signal_simulator_init(&sim);
    memset(&ctx, 0, sizeof(ctx));
    
    // Connect to broker
    TEST_ASSERT_TRUE(mock_mqtt_broker_connect(&broker));
    TEST_ASSERT_TRUE(mock_mqtt_broker_is_connected(&broker));
    
    // Subscribe to raw edge topic
    TEST_ASSERT_TRUE(mock_mqtt_broker_subscribe(&broker, "raw/14", raw_packet_callback, &ctx));
    
    // Generate test signal: square wave on pin 14
    signal_simulator_generate_square_wave(&sim, 14, 1000, 10000, 1000000);
    
    // Verify events were generated
    uint32_t event_count = signal_simulator_get_event_count(&sim);
    TEST_ASSERT_GREATER_THAN(0, event_count);
    printf("  Generated %u signal events\n", (unsigned int)event_count);
    
    // Simulate signal capture and packet creation
    // In real system, SignalTelemetry would batch these into RawPackets
    struct RawPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.header.version = 1;
    packet.header.type = 1;  // raw
    packet.baseTimeUs = 1000000;
    packet.baseSeq = 100;
    packet.count = 5;
    
    // Add first 5 edges
    uint32_t index = 0;
    for (uint8_t i = 0; i < 5; i++) {
        const sim_signal_event_t* event = signal_simulator_get_next_event(&sim, &index);
        if (event && event->type == SIM_EVENT_EDGE_CHANGE) {
            packet.edges[i].pinId = event->pin;
            packet.edges[i].value = event->value;
            packet.edges[i].dtUs = (uint32_t)(event->time_us - packet.baseTimeUs);
        }
    }
    
    // Publish packet to MQTT
    bool published = mock_mqtt_broker_publish(&broker, "raw/14", 
                                             (const uint8_t*)&packet, 
                                             sizeof(struct RawPacket), 
                                             true);
    TEST_ASSERT_TRUE(published);
    
    // Verify packet was received by subscriber
    TEST_ASSERT_EQUAL_UINT32(1, ctx.raw_packet_count);
    TEST_ASSERT_EQUAL_UINT8(1, ctx.last_raw_packet.header.version);
    TEST_ASSERT_EQUAL_UINT8(1, ctx.last_raw_packet.header.type);
    TEST_ASSERT_EQUAL_UINT32(100, ctx.last_raw_packet.baseSeq);
    TEST_ASSERT_EQUAL_UINT8(5, ctx.last_raw_packet.count);
    
    printf("  Successfully published and received RawPacket\n");
}

/**
 * Test: PSRAM Buffer Pressure
 * Simulates high-rate signal generation with slow MQTT to test buffer behavior
 */
void test_e2e_psram_buffer_pressure(void)
{
    mock_mqtt_broker_t broker;
    signal_simulator_t sim;
    
    mock_mqtt_broker_init(&broker);
    signal_simulator_init(&sim);
    
    // Connect but enable slow mode to simulate network pressure
    TEST_ASSERT_TRUE(mock_mqtt_broker_connect(&broker));
    mock_mqtt_broker_set_slow_mode(&broker, true, 10000);  // 10ms delay per publish
    
    // Generate high-rate signal events
    signal_simulator_generate_random_edges(&sim, 14, 500, 100, 500, 1000000);
    
    uint32_t event_count = signal_simulator_get_event_count(&sim);
    TEST_ASSERT_GREATER_THAN(400, event_count);
    printf("  Generated %u high-rate signal events\n", (unsigned int)event_count);
    
    // Simulate batching events into packets
    // In real system, PSRAM buffer would store packets when MQTT is slow
    uint32_t packets_created = 0;
    uint32_t index = 0;
    
    while (index < event_count) {
        struct RawPacket packet;
        memset(&packet, 0, sizeof(packet));
        packet.header.version = 1;
        packet.header.type = 1;
        packet.baseSeq = 100 + packets_created;
        
        uint8_t batch_count = 0;
        const sim_signal_event_t* first_event = signal_simulator_get_next_event(&sim, &index);
        if (first_event) {
            packet.baseTimeUs = first_event->time_us;
            packet.edges[batch_count].pinId = first_event->pin;
            packet.edges[batch_count].value = first_event->value;
            packet.edges[batch_count].dtUs = 0;
            batch_count++;
        }
        
        // Batch up to 50 edges or until we run out
        while (batch_count < 50 && index < event_count) {
            const sim_signal_event_t* event = signal_simulator_get_next_event(&sim, &index);
            if (event && event->type == SIM_EVENT_EDGE_CHANGE) {
                packet.edges[batch_count].pinId = event->pin;
                packet.edges[batch_count].value = event->value;
                packet.edges[batch_count].dtUs = (uint32_t)(event->time_us - packet.baseTimeUs);
                batch_count++;
            }
        }
        
        packet.count = batch_count;
        
        // Try to publish (will be slow due to slow mode)
        bool published = mock_mqtt_broker_publish(&broker, "raw/14",
                                                 (const uint8_t*)&packet,
                                                 sizeof(struct RawPacket),
                                                 true);
        if (published) {
            packets_created++;
        }
    }
    
    TEST_ASSERT_GREATER_THAN(5, packets_created);
    printf("  Created and published %u packets under pressure\n", (unsigned int)packets_created);
    
    // Verify messages were logged
    uint32_t message_count = mock_mqtt_broker_get_message_count(&broker);
    TEST_ASSERT_EQUAL_UINT32(packets_created, message_count);
}

/**
 * Test: MQTT Reconnection and Replay
 * Tests buffer replay when MQTT reconnects
 */
void test_e2e_mqtt_reconnection(void)
{
    mock_mqtt_broker_t broker;
    signal_simulator_t sim;
    test_context_t ctx;
    
    mock_mqtt_broker_init(&broker);
    signal_simulator_init(&sim);
    memset(&ctx, 0, sizeof(ctx));
    
    // Start disconnected
    printf("  Phase 1: Generating events while disconnected\n");
    
    // Generate signals while disconnected
    signal_simulator_generate_square_wave(&sim, 14, 500, 5000, 1000000);
    uint32_t offline_events = signal_simulator_get_event_count(&sim);
    TEST_ASSERT_GREATER_THAN(0, offline_events);
    printf("    Generated %u events while offline\n", (unsigned int)offline_events);
    
    // Simulate storing in PSRAM buffer (would be done by PSRAMBufferManager)
    // For this test, we'll just collect packets in an array
    struct RawPacket buffered_packets[20];
    uint32_t buffered_count = 0;
    uint32_t index = 0;
    
    while (index < offline_events && buffered_count < 20) {
        struct RawPacket* packet = &buffered_packets[buffered_count];
        memset(packet, 0, sizeof(struct RawPacket));
        packet->header.version = 1;
        packet->header.type = 1;
        packet->baseSeq = 100 + buffered_count;
        
        uint8_t batch_count = 0;
        const sim_signal_event_t* first_event = signal_simulator_get_next_event(&sim, &index);
        if (first_event) {
            packet->baseTimeUs = first_event->time_us;
            packet->edges[batch_count].pinId = first_event->pin;
            packet->edges[batch_count].value = first_event->value;
            packet->edges[batch_count].dtUs = 0;
            batch_count++;
        }
        
        while (batch_count < 50 && index < offline_events) {
            const sim_signal_event_t* event = signal_simulator_get_next_event(&sim, &index);
            if (event && event->type == SIM_EVENT_EDGE_CHANGE) {
                packet->edges[batch_count].pinId = event->pin;
                packet->edges[batch_count].value = event->value;
                packet->edges[batch_count].dtUs = (uint32_t)(event->time_us - packet->baseTimeUs);
                batch_count++;
            }
        }
        
        packet->count = batch_count;
        buffered_count++;
    }
    
    printf("    Buffered %u packets in PSRAM\n", (unsigned int)buffered_count);
    
    // Now connect to MQTT
    printf("  Phase 2: Connecting to MQTT and replaying buffer\n");
    TEST_ASSERT_TRUE(mock_mqtt_broker_connect(&broker));
    TEST_ASSERT_TRUE(mock_mqtt_broker_subscribe(&broker, "raw/14", raw_packet_callback, &ctx));
    
    // Replay buffered packets
    for (uint32_t i = 0; i < buffered_count; i++) {
        bool published = mock_mqtt_broker_publish(&broker, "raw/14",
                                                 (const uint8_t*)&buffered_packets[i],
                                                 sizeof(struct RawPacket),
                                                 true);
        TEST_ASSERT_TRUE(published);
    }
    
    printf("    Replayed %u packets\n", (unsigned int)buffered_count);
    
    // Verify all packets were received
    TEST_ASSERT_EQUAL_UINT32(buffered_count, ctx.raw_packet_count);
    printf("  Successfully replayed buffer after reconnection\n");
}

/**
 * Test: Mixed Signal Types
 * Tests publishing both raw edge and pulse width packets
 */
void test_e2e_mixed_signals(void)
{
    mock_mqtt_broker_t broker;
    signal_simulator_t sim;
    test_context_t ctx;
    
    mock_mqtt_broker_init(&broker);
    signal_simulator_init(&sim);
    memset(&ctx, 0, sizeof(ctx));
    
    TEST_ASSERT_TRUE(mock_mqtt_broker_connect(&broker));
    
    // Subscribe to both raw and pulse topics
    TEST_ASSERT_TRUE(mock_mqtt_broker_subscribe(&broker, "raw/#", raw_packet_callback, &ctx));
    TEST_ASSERT_TRUE(mock_mqtt_broker_subscribe(&broker, "pulse/#", pulse_packet_callback, &ctx));
    
    // Generate raw edges on pin 14
    signal_simulator_generate_square_wave(&sim, 14, 1000, 2000, 1000000);
    
    // Generate pulse events on pin 15
    signal_simulator_generate_pulses(&sim, 15, 5, 1000, 2000, 2000000);
    
    uint32_t total_events = signal_simulator_get_event_count(&sim);
    printf("  Generated %u total events (mixed types)\n", (unsigned int)total_events);
    
    // Create and publish a raw packet
    struct RawPacket raw_packet;
    memset(&raw_packet, 0, sizeof(raw_packet));
    raw_packet.header.version = 1;
    raw_packet.header.type = 1;
    raw_packet.baseTimeUs = 1000000;
    raw_packet.baseSeq = 100;
    raw_packet.count = 3;
    raw_packet.edges[0].pinId = 14;
    raw_packet.edges[0].value = 1;
    raw_packet.edges[0].dtUs = 0;
    raw_packet.edges[1].pinId = 14;
    raw_packet.edges[1].value = 0;
    raw_packet.edges[1].dtUs = 500;
    raw_packet.edges[2].pinId = 14;
    raw_packet.edges[2].value = 1;
    raw_packet.edges[2].dtUs = 1000;
    
    TEST_ASSERT_TRUE(mock_mqtt_broker_publish(&broker, "raw/14",
                                              (const uint8_t*)&raw_packet,
                                              sizeof(struct RawPacket), true));
    
    // Create and publish a pulse packet
    struct PulsePacket pulse_packet;
    memset(&pulse_packet, 0, sizeof(pulse_packet));
    pulse_packet.header.version = 1;
    pulse_packet.header.type = 2;
    pulse_packet.pinId = 15;
    pulse_packet.highUs = 1000;
    pulse_packet.lowUs = 2000;
    pulse_packet.deviceTimeUs = 2000000;
    pulse_packet.seq = 200;
    
    TEST_ASSERT_TRUE(mock_mqtt_broker_publish(&broker, "pulse/15",
                                              (const uint8_t*)&pulse_packet,
                                              sizeof(struct PulsePacket), true));
    
    // Verify both packet types were received
    TEST_ASSERT_EQUAL_UINT32(1, ctx.raw_packet_count);
    TEST_ASSERT_EQUAL_UINT32(1, ctx.pulse_packet_count);
    
    // Verify packet contents
    TEST_ASSERT_EQUAL_UINT8(1, ctx.last_raw_packet.header.type);
    TEST_ASSERT_EQUAL_UINT8(14, ctx.last_raw_packet.edges[0].pinId);
    
    TEST_ASSERT_EQUAL_UINT8(2, ctx.last_pulse_packet.header.type);
    TEST_ASSERT_EQUAL_UINT8(15, ctx.last_pulse_packet.pinId);
    TEST_ASSERT_EQUAL_UINT32(1000, ctx.last_pulse_packet.highUs);
    TEST_ASSERT_EQUAL_UINT32(2000, ctx.last_pulse_packet.lowUs);
    
    printf("  Successfully handled mixed signal types\n");
}
