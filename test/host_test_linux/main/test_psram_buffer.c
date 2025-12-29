/*
 * ESP32 Vault - PSRAM Buffer Unit Tests
 * 
 * Tests for PSRAM buffer manager functionality.
 * Note: On Linux host, these tests use regular malloc instead of PSRAM.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "unity.h"

// Simple buffer structure for testing
#define TEST_BUFFER_SIZE 100
#define TEST_PACKET_SIZE 16

typedef struct {
    uint8_t data[TEST_PACKET_SIZE];
    uint32_t seq;
} TestPacket;

typedef struct {
    TestPacket* packets;
    size_t capacity;
    size_t count;
    size_t writeIndex;
    size_t readIndex;
    uint32_t dropped;
} TestBuffer;

// Simple circular buffer implementation for testing
TestBuffer* test_buffer_create(size_t capacity)
{
    TestBuffer* buffer = (TestBuffer*)malloc(sizeof(TestBuffer));
    if (!buffer) return NULL;
    
    buffer->packets = (TestPacket*)malloc(capacity * sizeof(TestPacket));
    if (!buffer->packets) {
        free(buffer);
        return NULL;
    }
    
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->writeIndex = 0;
    buffer->readIndex = 0;
    buffer->dropped = 0;
    
    return buffer;
}

void test_buffer_destroy(TestBuffer* buffer)
{
    if (buffer) {
        if (buffer->packets) {
            free(buffer->packets);
        }
        free(buffer);
    }
}

int test_buffer_write(TestBuffer* buffer, const TestPacket* packet)
{
    if (!buffer || !packet) return -1;
    
    if (buffer->count >= buffer->capacity) {
        buffer->dropped++;
        return -1; // Buffer full
    }
    
    memcpy(&buffer->packets[buffer->writeIndex], packet, sizeof(TestPacket));
    buffer->writeIndex = (buffer->writeIndex + 1) % buffer->capacity;
    buffer->count++;
    
    return 0;
}

int test_buffer_read(TestBuffer* buffer, TestPacket* packet)
{
    if (!buffer || !packet) return -1;
    
    if (buffer->count == 0) {
        return -1; // Buffer empty
    }
    
    memcpy(packet, &buffer->packets[buffer->readIndex], sizeof(TestPacket));
    buffer->readIndex = (buffer->readIndex + 1) % buffer->capacity;
    buffer->count--;
    
    return 0;
}

size_t test_buffer_available(TestBuffer* buffer)
{
    return buffer ? buffer->count : 0;
}

void test_psram_buffer_basic(void)
{
    // Create a small buffer
    TestBuffer* buffer = test_buffer_create(10);
    TEST_ASSERT_NOT_NULL(buffer);
    
    // Verify initial state
    TEST_ASSERT_EQUAL_UINT(0, test_buffer_available(buffer));
    TEST_ASSERT_EQUAL_UINT(0, buffer->dropped);
    
    // Write a packet
    TestPacket packet1;
    memset(&packet1, 0, sizeof(packet1));
    packet1.seq = 100;
    packet1.data[0] = 0xAA;
    packet1.data[1] = 0xBB;
    
    int result = test_buffer_write(buffer, &packet1);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(1, test_buffer_available(buffer));
    
    // Read the packet back
    TestPacket packet2;
    result = test_buffer_read(buffer, &packet2);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(0, test_buffer_available(buffer));
    
    // Verify packet contents
    TEST_ASSERT_EQUAL_UINT32(100, packet2.seq);
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet2.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, packet2.data[1]);
    
    // Clean up
    test_buffer_destroy(buffer);
}

void test_psram_buffer_overflow(void)
{
    // Create a small buffer
    TestBuffer* buffer = test_buffer_create(5);
    TEST_ASSERT_NOT_NULL(buffer);
    
    // Fill the buffer
    TestPacket packet;
    for (int i = 0; i < 5; i++) {
        memset(&packet, 0, sizeof(packet));
        packet.seq = i;
        int result = test_buffer_write(buffer, &packet);
        TEST_ASSERT_EQUAL_INT(0, result);
    }
    
    TEST_ASSERT_EQUAL_UINT(5, test_buffer_available(buffer));
    TEST_ASSERT_EQUAL_UINT(0, buffer->dropped);
    
    // Try to write one more (should fail)
    memset(&packet, 0, sizeof(packet));
    packet.seq = 999;
    int result = test_buffer_write(buffer, &packet);
    TEST_ASSERT_EQUAL_INT(-1, result);
    TEST_ASSERT_EQUAL_UINT(5, test_buffer_available(buffer));
    TEST_ASSERT_EQUAL_UINT(1, buffer->dropped);
    
    // Read one packet
    result = test_buffer_read(buffer, &packet);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT32(0, packet.seq); // First packet
    TEST_ASSERT_EQUAL_UINT(4, test_buffer_available(buffer));
    
    // Now we should be able to write again
    memset(&packet, 0, sizeof(packet));
    packet.seq = 1000;
    result = test_buffer_write(buffer, &packet);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(5, test_buffer_available(buffer));
    
    // Dropped count should remain 1
    TEST_ASSERT_EQUAL_UINT(1, buffer->dropped);
    
    // Clean up
    test_buffer_destroy(buffer);
}
