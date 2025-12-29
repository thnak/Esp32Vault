#ifndef PSRAM_BUFFER_MANAGER_H
#define PSRAM_BUFFER_MANAGER_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Forward declaration
struct RawPacket;

/**
 * PSRAMBufferManager - Offline telemetry buffer using PSRAM
 * 
 * Design:
 * - Fixed-size circular buffer pre-allocated at boot
 * - Stores complete RawPacket structures for replay
 * - Oldest-drop policy when full
 * - NOT used by ISR (ISR safety - ISR writes to SRAM ring buffer)
 * - Only used for backlog/replay, not realtime capture
 * 
 * Usage:
 * - When MQTT is connected and fast: direct publish, bypass PSRAM
 * - When MQTT is slow: spill to PSRAM
 * - When MQTT is down: 100% to PSRAM
 * - When MQTT reconnects: replay from PSRAM in sequence order
 */
class PSRAMBufferManager {
private:
    // PSRAM buffer configuration
    static const size_t MAX_PACKETS = 8192;  // ~4MB for packets (8192 * 500 bytes avg)
    
    // Circular buffer storage (allocated in PSRAM)
    RawPacket* buffer;
    
    // Circular buffer indices
    volatile uint32_t writeIndex;
    volatile uint32_t readIndex;
    volatile uint32_t count;  // Number of packets in buffer
    
    // Statistics
    uint32_t droppedPackets;  // Counter for oldest-drop events
    uint32_t totalWritten;
    uint32_t totalReplayed;
    
    // Thread safety
    SemaphoreHandle_t mutex;
    
    // Helper methods
    bool isFull() const;
    bool isEmpty() const;
    uint32_t getAvailableSpace() const;
    
public:
    PSRAMBufferManager();
    ~PSRAMBufferManager();
    
    /**
     * Initialize PSRAM buffer
     * Must be called at boot before use
     * Allocates buffer in PSRAM
     * Returns true if successful
     */
    bool begin();
    
    /**
     * Add packet to PSRAM buffer
     * Uses oldest-drop policy if full
     * Returns true if packet stored, false if error
     */
    bool enqueue(const RawPacket* packet);
    
    /**
     * Get next packet for replay
     * Returns pointer to packet (must be copied immediately)
     * Returns nullptr if buffer is empty
     */
    const RawPacket* dequeue();
    
    /**
     * Peek at next packet without removing
     * Returns nullptr if buffer is empty
     */
    const RawPacket* peek() const;
    
    /**
     * Get buffer statistics
     */
    uint32_t getCount() const { return count; }
    uint32_t getDroppedCount() const { return droppedPackets; }
    uint32_t getTotalWritten() const { return totalWritten; }
    uint32_t getTotalReplayed() const { return totalReplayed; }
    float getUsagePercent() const;
    
    /**
     * Clear all buffered packets
     */
    void clear();
    
    /**
     * Check if PSRAM is available and initialized
     */
    bool isInitialized() const { return buffer != nullptr; }
};

#endif // PSRAM_BUFFER_MANAGER_H
