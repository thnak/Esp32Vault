#include "PSRAMBufferManager.h"
#include "SignalTelemetry.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstring>

static const char *TAG = "PSRAMBufferManager";

PSRAMBufferManager::PSRAMBufferManager() 
    : buffer(nullptr), writeIndex(0), readIndex(0), count(0),
      droppedPackets(0), totalWritten(0), totalReplayed(0), mutex(nullptr) {
}

PSRAMBufferManager::~PSRAMBufferManager() {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
        buffer = nullptr;
    }
    
    if (mutex != nullptr) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}

bool PSRAMBufferManager::begin() {
    // Check if PSRAM is available
    size_t psramSize = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psramSize == 0) {
        ESP_LOGE(TAG, "PSRAM not available!");
        return false;
    }
    
    ESP_LOGI(TAG, "PSRAM available: %d bytes", psramSize);
    
    // Create mutex for thread safety
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    // Allocate buffer in PSRAM
    size_t bufferSize = MAX_PACKETS * sizeof(RawPacket);
    buffer = (RawPacket*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer (%d bytes)", bufferSize);
        vSemaphoreDelete(mutex);
        mutex = nullptr;
        return false;
    }
    
    // Initialize buffer
    memset(buffer, 0, bufferSize);
    writeIndex = 0;
    readIndex = 0;
    count = 0;
    droppedPackets = 0;
    totalWritten = 0;
    totalReplayed = 0;
    
    ESP_LOGI(TAG, "PSRAM buffer initialized: %d packets (%d bytes)", MAX_PACKETS, bufferSize);
    ESP_LOGI(TAG, "Buffer usage policy: Circular buffer with oldest-drop");
    
    return true;
}

bool PSRAMBufferManager::enqueue(const RawPacket* packet) {
    if (buffer == nullptr || packet == nullptr) {
        return false;
    }
    
    // Take mutex
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex for enqueue");
        return false;
    }
    
    // Check if buffer is full
    if (isFull()) {
        // Drop oldest packet (advance read index)
        readIndex = (readIndex + 1) % MAX_PACKETS;
        count--;
        droppedPackets++;
        
        ESP_LOGW(TAG, "PSRAM buffer full, dropped oldest packet (total dropped: %d)", droppedPackets);
    }
    
    // Copy packet to buffer
    memcpy(&buffer[writeIndex], packet, sizeof(RawPacket));
    
    // Advance write index
    writeIndex = (writeIndex + 1) % MAX_PACKETS;
    count++;
    totalWritten++;
    
    xSemaphoreGive(mutex);
    
    return true;
}

const RawPacket* PSRAMBufferManager::dequeue() {
    if (buffer == nullptr || isEmpty()) {
        return nullptr;
    }
    
    // Take mutex
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex for dequeue");
        return nullptr;
    }
    
    // Get packet at read index
    const RawPacket* packet = &buffer[readIndex];
    
    // Advance read index
    readIndex = (readIndex + 1) % MAX_PACKETS;
    count--;
    totalReplayed++;
    
    xSemaphoreGive(mutex);
    
    return packet;
}

const RawPacket* PSRAMBufferManager::peek() const {
    if (buffer == nullptr || isEmpty()) {
        return nullptr;
    }
    
    // Take mutex for thread safety
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return nullptr;
    }
    
    const RawPacket* packet = &buffer[readIndex];
    
    xSemaphoreGive(mutex);
    
    return packet;
}

bool PSRAMBufferManager::isFull() const {
    return count >= MAX_PACKETS;
}

bool PSRAMBufferManager::isEmpty() const {
    return count == 0;
}

uint32_t PSRAMBufferManager::getAvailableSpace() const {
    return MAX_PACKETS - count;
}

float PSRAMBufferManager::getUsagePercent() const {
    return (float)count / (float)MAX_PACKETS * 100.0f;
}

void PSRAMBufferManager::clear() {
    if (buffer == nullptr) {
        return;
    }
    
    // Take mutex
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex for clear");
        return;
    }
    
    writeIndex = 0;
    readIndex = 0;
    count = 0;
    
    xSemaphoreGive(mutex);
    
    ESP_LOGI(TAG, "PSRAM buffer cleared");
}
