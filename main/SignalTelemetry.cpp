#include "SignalTelemetry.h"
#include "MQTTManager.h"
#include "PSRAMBufferManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char *TAG = "SignalTelemetry";

// Helper function for milliseconds timestamp
static inline unsigned long millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

// Static member initialization
std::map<uint8_t, SignalTelemetry*> SignalTelemetry::isrHandlers;

SignalTelemetry::SignalTelemetry() 
    : mqttManager(nullptr), psramBuffer(nullptr), seq(0), droppedRaw(0), droppedPulse(0), rmtOverflow(0),
      mqttState(MQTTState::DISCONNECTED), lastPublishSuccess(0), lastPublishAttempt(0),
      replayInProgress(false), replayTaskHandle(nullptr),
      signalRingBuffer(nullptr), batchQueue(nullptr), 
      collectTaskHandle(nullptr), publishTaskHandle(nullptr) {
}

SignalTelemetry::~SignalTelemetry() {
    // Cleanup interrupts and RMT
    for (auto& pair : configuredPins) {
        gpio_isr_handler_remove((gpio_num_t)pair.first);
        if (pair.second.useRMT) {
            cleanupRMT(pair.first);
        }
    }
    
    // Delete tasks
    if (replayTaskHandle != nullptr) {
        vTaskDelete(replayTaskHandle);
    }
    if (collectTaskHandle != nullptr) {
        vTaskDelete(collectTaskHandle);
    }
    if (publishTaskHandle != nullptr) {
        vTaskDelete(publishTaskHandle);
    }
    
    // Delete ring buffer and queue
    if (signalRingBuffer != nullptr) {
        vRingbufferDelete(signalRingBuffer);
    }
    if (batchQueue != nullptr) {
        vQueueDelete(batchQueue);
    }
    
    // Delete PSRAM buffer
    if (psramBuffer != nullptr) {
        delete psramBuffer;
    }
}

void SignalTelemetry::begin(MQTTManager* mqtt, const std::string& macAddr) {
    mqttManager = mqtt;
    macAddress = macAddr;
    
    // Initialize PSRAM buffer
    psramBuffer = new PSRAMBufferManager();
    if (!psramBuffer->begin()) {
        ESP_LOGE(TAG, "CRITICAL: PSRAM buffer initialization failed - offline buffering disabled!");
        delete psramBuffer;
        psramBuffer = nullptr;
    } else {
        ESP_LOGI(TAG, "PSRAM offline buffer initialized successfully");
    }
    
    // Create lock-free ring buffer for ISR events
    signalRingBuffer = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (signalRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Failed to create signal ring buffer");
        return;
    }
    
    // Create batch queue
    batchQueue = xQueueCreate(BATCH_QUEUE_SIZE, sizeof(RawPacket));
    if (batchQueue == nullptr) {
        ESP_LOGE(TAG, "ERROR: Failed to create batch queue");
        return;
    }
    
    // Create high-priority signal collect task
    BaseType_t result = xTaskCreate(
        collectTaskFunction,
        "SignalCollect",
        8192,              // Stack size
        this,
        10,                // High priority
        &collectTaskHandle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "ERROR: Failed to create signal collect task");
        return;
    }
    
    // Create low-priority MQTT publish task
    result = xTaskCreate(
        publishTaskFunction,
        "SignalPublish",
        4096,              // Stack size
        this,
        3,                 // Low priority
        &publishTaskHandle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "ERROR: Failed to create signal publish task");
        return;
    }
    
    ESP_LOGI(TAG, "SignalTelemetry initialized");
}

void SignalTelemetry::loop() {
    // Main loop doesn't do much - tasks handle everything
    // Could add periodic diagnostics here
    static unsigned long lastDiag = 0;
    unsigned long now = millis();
    
    if (now - lastDiag > 60000) { // Every 60 seconds
        lastDiag = now;
        publishDiagnostics();
    }
}

bool SignalTelemetry::configurePin(uint8_t pin, bool captureRaw, bool capturePulse, bool useRMT) {
    // Remove old configuration if exists
    if (configuredPins.find(pin) != configuredPins.end()) {
        removePin(pin);
    }
    
    SignalPinConfig config;
    config.pin = pin;
    config.captureRaw = captureRaw;
    config.capturePulse = capturePulse;
    config.useRMT = useRMT;
    config.rawTopic = "raw/" + std::to_string(pin);
    config.pulseTopic = "pulse/" + std::to_string(pin);
    
// <<<<<<< copilot/implement-offline-buffering-psram
    // Configure hardware using ESP-IDF GPIO API
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
// =======
    // Configure hardware
    pinMode(pin, INPUT);
// >>>>>>> main
    
    // Setup RMT if requested and capturing pulse
    if (capturePulse && useRMT) {
        // Try to allocate RMT channel (0-7 available on ESP32)
        // For simplicity, use pin number modulo 8 as channel
        if (configureRMT(pin, &config.rmtChannel)) {
            // RMT configured successfully
        } else {
// <<<<<<< copilot/implement-offline-buffering-psram
            ESP_LOGW(TAG, "RMT configuration failed for pin %d, using ISR fallback", pin);
// =======
            ESP_LOGW(TAG, "WARNING: RMT configuration failed for pin %u, using ISR fallback", pin);
// >>>>>>> main
            config.useRMT = false;
        }
    }
    
    // Attach interrupt for raw edge capture (always attach if captureRaw or capturePulse without RMT)
    if (captureRaw || (capturePulse && !config.useRMT)) {
        // Install ISR service if not already installed
        static bool isrServiceInstalled = false;
        if (!isrServiceInstalled) {
            gpio_install_isr_service(0);
            isrServiceInstalled = true;
        }
        
        isrHandlers[pin] = this;
        gpio_isr_handler_add((gpio_num_t)pin, edgeISR, (void*)(uintptr_t)pin);
    }
    
    configuredPins[pin] = config;
    
// <<<<<<< copilot/implement-offline-buffering-psram
    ESP_LOGI(TAG, "Pin %d configured for signal telemetry", pin);
// =======
    ESP_LOGI(TAG, "Pin %u configured for signal telemetry", pin);
// >>>>>>> main
    ESP_LOGI(TAG, "  Raw: %s", captureRaw ? "Yes" : "No");
    ESP_LOGI(TAG, "  Pulse: %s", capturePulse ? "Yes" : "No");
    ESP_LOGI(TAG, "  RMT: %s", config.useRMT ? "Yes" : "No");
    
    return true;
}

bool SignalTelemetry::removePin(uint8_t pin) {
    auto it = configuredPins.find(pin);
    if (it == configuredPins.end()) {
        return false;
    }
    
    // Detach interrupt
    gpio_isr_handler_remove((gpio_num_t)pin);
    isrHandlers.erase(pin);
    
    // Cleanup RMT if used
    if (it->second.useRMT) {
        cleanupRMT(pin);
    }
    
    configuredPins.erase(it);
    
// <<<<<<< copilot/implement-offline-buffering-psram
    ESP_LOGI(TAG, "Pin %d removed from signal telemetry", pin);
// =======
    ESP_LOGI(TAG, "Pin %u removed from signal telemetry", pin);
// >>>>>>> main
    return true;
}

void IRAM_ATTR SignalTelemetry::edgeISR(void* arg) {
    uint8_t pin = (uint8_t)(uintptr_t)arg;
    
    auto it = isrHandlers.find(pin);
    if (it == isrHandlers.end()) {
        return;
    }
    
    SignalTelemetry* instance = it->second;
    
    // Read value and timestamp with minimal latency
    uint8_t value = gpio_get_level((gpio_num_t)pin);
    uint64_t timeUs = esp_timer_get_time();
    
    // Create event
    SignalEvent event;
    event.type = SignalEventType::EDGE_CHANGE;
    event.pinId = pin;
    event.value = value;
    event.deviceTimeUs = timeUs;
    event.highUs = 0;
    event.lowUs = 0;
    
    // Try to send to ring buffer (no block in ISR)
    BaseType_t result = xRingbufferSendFromISR(instance->signalRingBuffer, &event, sizeof(SignalEvent), NULL);
    
    if (result != pdTRUE) {
        // Ring buffer full - count as dropped
        instance->droppedRaw++;
    }
}

void SignalTelemetry::collectTaskFunction(void* parameter) {
    SignalTelemetry* instance = static_cast<SignalTelemetry*>(parameter);
    
    RawPacket currentBatch;
    currentBatch.header.version = 1;
    currentBatch.header.type = 1; // raw
    currentBatch.count = 0;
    currentBatch.baseTimeUs = 0;
    currentBatch.baseSeq = 0;
    
    unsigned long lastBatchTime = millis();
    
    while (true) {
        // Try to receive from ring buffer with short timeout
        size_t itemSize;
        SignalEvent* event = (SignalEvent*)xRingbufferReceive(
            instance->signalRingBuffer, 
            &itemSize, 
            pdMS_TO_TICKS(10)
        );
        
        if (event != nullptr) {
            // Process event based on type
            if (event->type == SignalEventType::EDGE_CHANGE) {
                // Initialize batch if empty
                if (currentBatch.count == 0) {
                    currentBatch.baseTimeUs = event->deviceTimeUs;
                    currentBatch.baseSeq = instance->seq;
                    lastBatchTime = millis();
                }
                
                // Add to batch
                RawEdge& edge = currentBatch.edges[currentBatch.count];
                edge.pinId = event->pinId;
                edge.value = event->value;
                edge.dtUs = (uint32_t)(event->deviceTimeUs - currentBatch.baseTimeUs);
                currentBatch.count++;
                
                instance->seq++;
                
                // Check if batch is full or time window exceeded
                unsigned long now = millis();
                if (currentBatch.count >= instance->MAX_BATCH_SIZE || 
                    (now - lastBatchTime) >= instance->MAX_BATCH_TIME_MS) {
                    
                    // Try to queue batch
                    if (xQueueSend(instance->batchQueue, &currentBatch, 0) != pdTRUE) {
                        // Queue full - drop oldest and try again
                        RawPacket dummy;
                        xQueueReceive(instance->batchQueue, &dummy, 0);
                        instance->droppedRaw += dummy.count;
                        xQueueSend(instance->batchQueue, &currentBatch, 0);
                    }
                    
                    // Reset batch
                    currentBatch.count = 0;
                }
            }
            else if (event->type == SignalEventType::PULSE_WIDTH) {
                // Create pulse packet
                PulsePacket pulse;
                pulse.header.version = 1;
                pulse.header.type = 2; // pulse
                pulse.pinId = event->pinId;
                pulse.highUs = event->highUs;
                pulse.lowUs = event->lowUs;
                pulse.deviceTimeUs = event->deviceTimeUs;
                pulse.seq = instance->getNextSeq();
                
                instance->publishPulse(&pulse);
            }
            
            // Return item to ring buffer
            vRingbufferReturnItem(instance->signalRingBuffer, (void*)event);
        }
        
        // Check if we need to flush partial batch due to timeout
        if (currentBatch.count > 0) {
            unsigned long now = millis();
            if ((now - lastBatchTime) >= instance->MAX_BATCH_TIME_MS) {
                // Queue partial batch
                if (xQueueSend(instance->batchQueue, &currentBatch, 0) != pdTRUE) {
                    // Queue full - drop oldest
                    RawPacket dummy;
                    xQueueReceive(instance->batchQueue, &dummy, 0);
                    instance->droppedRaw += dummy.count;
                    xQueueSend(instance->batchQueue, &currentBatch, 0);
                }
                currentBatch.count = 0;
            }
        }
    }
}

void SignalTelemetry::publishTaskFunction(void* parameter) {
    SignalTelemetry* instance = static_cast<SignalTelemetry*>(parameter);
    RawPacket batch;
    
    while (true) {
        // Wait for batch to publish
        if (xQueueReceive(instance->batchQueue, &batch, portMAX_DELAY) == pdTRUE) {
            // Update MQTT state
            instance->updateMQTTState();
            
            // Decision logic based on MQTT state
            if (instance->shouldUseDirectPublish()) {
                // Try direct publish
                if (!instance->tryDirectPublish(&batch)) {
                    // Direct publish failed, spill to PSRAM
                    instance->spillToPSRAM(&batch);
                }
            } else {
                // MQTT is slow or down, spill to PSRAM
                instance->spillToPSRAM(&batch);
            }
        }
    }
}

void SignalTelemetry::publishRawBatch(const RawPacket* batch) {
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
        droppedRaw += batch->count;
        return;
    }
    
    // Get topic from first edge's pin
    if (batch->count == 0) {
        return;
    }
    
    uint8_t pinId = batch->edges[0].pinId;
    std::string topic = "raw/" + std::to_string(pinId);
    
    // Calculate payload size
    size_t headerSize = sizeof(PacketHeader) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint8_t);
    size_t payloadSize = headerSize + (batch->count * sizeof(RawEdge));
    
    // Publish binary payload with MQTT5 properties
    // Set message expiry for time-sensitive telemetry
    mqttManager->publishBinary(topic, (const uint8_t*)batch, payloadSize, 
                              CONTENT_TYPE_RAW_SIGNAL, false, MESSAGE_EXPIRY_TELEMETRY_SECONDS);
}

void SignalTelemetry::publishPulse(const PulsePacket* pulse) {
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
        droppedPulse++;
        return;
    }
    
    std::string topic = "pulse/" + std::to_string(pulse->pinId);
    
    // Publish binary pulse packet with MQTT5 properties
    // Set message expiry for time-sensitive telemetry
    mqttManager->publishBinary(topic, (const uint8_t*)pulse, sizeof(PulsePacket), 
                              CONTENT_TYPE_PULSE_SIGNAL, false, MESSAGE_EXPIRY_TELEMETRY_SECONDS);
}

void SignalTelemetry::publishDiagnostics() {
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
        return;
    }
    
    DiagPacket diag;
    diag.header.version = 1;
    diag.header.type = 3; // diag
    diag.droppedRaw = droppedRaw;
    diag.droppedPulse = droppedPulse;
    diag.queueDepth = (uint16_t)uxQueueMessagesWaiting(batchQueue);
    diag.rmtOverflow = rmtOverflow;
    
    // Publish binary diagnostic packet with MQTT5 properties
    mqttManager->publishBinary("diag", (const uint8_t*)&diag, sizeof(DiagPacket), 
                              CONTENT_TYPE_DIAG_SIGNAL, false, 0);
    
    ESP_LOGI(TAG, "Diagnostics published:");
// <<<<<<< copilot/implement-offline-buffering-psram
    ESP_LOGI(TAG, "  Dropped Raw: %d", diag.droppedRaw);
    ESP_LOGI(TAG, "  Dropped Pulse: %d", diag.droppedPulse);
    ESP_LOGI(TAG, "  Queue Depth: %d", diag.queueDepth);
    ESP_LOGI(TAG, "  RMT Overflow: %d", diag.rmtOverflow);
// =======
    ESP_LOGI(TAG, "  Dropped Raw: %u", diag.droppedRaw);
    ESP_LOGI(TAG, "  Dropped Pulse: %u", diag.droppedPulse);
    ESP_LOGI(TAG, "  Queue Depth: %u", diag.queueDepth);
    ESP_LOGI(TAG, "  RMT Overflow: %u", diag.rmtOverflow);
// >>>>>>> main
}

void SignalTelemetry::publishHeartbeat() {
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
        return;
    }
    
    std::string payload = "{\"mac\":\"" + macAddress + "\",\"seq\":" + std::to_string(seq) + ",\"uptime\":" + std::to_string(millis()/1000) + "}";
    mqttManager->publish("heartbeat", payload, false);
}

void SignalTelemetry::onBoot() {
    // Reset sequence counter
    seq = 0;
    droppedRaw = 0;
    droppedPulse = 0;
    rmtOverflow = 0;
    
    // Publish heartbeat
    publishHeartbeat();
    
    // Publish initial diagnostics
    publishDiagnostics();
    
    ESP_LOGI(TAG, "SignalTelemetry boot sequence complete");
}

uint32_t SignalTelemetry::getNextSeq() {
    return seq++;
}

DiagPacket SignalTelemetry::getDiagnostics() {
    DiagPacket diag;
    diag.header.version = 1;
    diag.header.type = 3;
    diag.droppedRaw = droppedRaw;
    diag.droppedPulse = droppedPulse;
    diag.queueDepth = batchQueue ? (uint16_t)uxQueueMessagesWaiting(batchQueue) : 0;
    diag.rmtOverflow = rmtOverflow;
    return diag;
}

// <<<<<<< copilot/implement-offline-buffering-psram
void SignalTelemetry::updateMQTTState() {
    MQTTState previousState = mqttState;
    
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
        mqttState = MQTTState::DISCONNECTED;
        return;
    }
    
    // MQTT is connected, check if it's fast or slow
    unsigned long now = millis();
    if (lastPublishAttempt > 0 && (now - lastPublishAttempt) > MQTT_SLOW_THRESHOLD_MS) {
        mqttState = MQTTState::CONNECTED_SLOW;
        ESP_LOGW(TAG, "MQTT appears slow (>%lu ms since last attempt)", MQTT_SLOW_THRESHOLD_MS);
    } else {
        mqttState = MQTTState::CONNECTED_FAST;
    }
    
    // Start replay when transitioning from DISCONNECTED to CONNECTED
    if (previousState == MQTTState::DISCONNECTED && 
        (mqttState == MQTTState::CONNECTED_FAST || mqttState == MQTTState::CONNECTED_SLOW)) {
        if (!replayInProgress && psramBuffer != nullptr && psramBuffer->getCount() > 0) {
            ESP_LOGI(TAG, "MQTT reconnected with %d buffered packets, starting replay task", 
                     psramBuffer->getCount());
            startReplay();
        }
    }
}
}

bool SignalTelemetry::shouldUseDirectPublish() const {
    // Only use direct publish if MQTT is connected and fast
    // AND there's no backlog in PSRAM (replay has priority)
    if (mqttState == MQTTState::CONNECTED_FAST) {
        if (psramBuffer != nullptr && psramBuffer->getCount() > 0) {
            // Has backlog, replay first
            return false;
        }
        return true;
// =======
bool SignalTelemetry::configureRMT(uint8_t pin, rmt_channel_t channel) {
    rmt_config_t config;
    config.rmt_mode = RMT_MODE_RX;
    config.channel = channel;
    config.gpio_num = (gpio_num_t)pin;
    config.clk_div = 80; // 1us resolution (80MHz / 80 = 1MHz)
    config.mem_block_num = 1;
    config.flags = 0;
    
    config.rx_config.filter_en = false;
    config.rx_config.filter_ticks_thresh = 0;
    config.rx_config.idle_threshold = 65535; // Max idle threshold
    
    esp_err_t err = rmt_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ERROR: RMT config failed: 0x%x", err);
        return false;
    }
    
    err = rmt_driver_install(channel, 1000, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ERROR: RMT driver install failed: 0x%x", err);
        return false;
// >>>>>>> main
    }
    return false;
}

bool SignalTelemetry::tryDirectPublish(const RawPacket* batch) {
    lastPublishAttempt = millis();
    
// <<<<<<< copilot/implement-offline-buffering-psram
    if (mqttManager == nullptr || !mqttManager->isConnected()) {
// =======
    // Start receiving
    err = rmt_rx_start(channel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ERROR: RMT rx start failed: 0x%x", err);
        rmt_driver_uninstall(channel);
// >>>>>>> main
        return false;
    }
    
    // Attempt to publish
    publishRawBatch(batch);
    
    lastPublishSuccess = millis();
    return true;
}

void SignalTelemetry::spillToPSRAM(const RawPacket* batch) {
    if (psramBuffer == nullptr) {
        // No PSRAM available, must drop
        droppedRaw += batch->count;
        ESP_LOGW(TAG, "No PSRAM buffer, dropping %d edges", batch->count);
        return;
    }
    
    // Enqueue to PSRAM
    if (!psramBuffer->enqueue(batch)) {
        droppedRaw += batch->count;
        ESP_LOGE(TAG, "Failed to enqueue to PSRAM, dropping %d edges", batch->count);
    }
}

void SignalTelemetry::startReplay() {
    if (replayInProgress || psramBuffer == nullptr) {
        return;
    }
    
    replayInProgress = true;
    
    // Create replay task with low priority (lower than publish task)
    BaseType_t result = xTaskCreate(
        replayTaskFunction,
        "SignalReplay",
        4096,
        this,
        2,  // Lower priority than publish task
        &replayTaskHandle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create replay task");
        replayInProgress = false;
    }
}

void SignalTelemetry::stopReplay() {
    if (!replayInProgress) {
        return;
    }
    
    if (replayTaskHandle != nullptr) {
        vTaskDelete(replayTaskHandle);
        replayTaskHandle = nullptr;
    }
    
    replayInProgress = false;
    ESP_LOGI(TAG, "Replay task stopped");
}

void SignalTelemetry::replayTaskFunction(void* parameter) {
    SignalTelemetry* instance = static_cast<SignalTelemetry*>(parameter);
    
    ESP_LOGI(TAG, "Replay task started, packets to replay: %d", 
             instance->psramBuffer->getCount());
    
    while (instance->psramBuffer != nullptr && instance->psramBuffer->getCount() > 0) {
        // Check if MQTT is still connected
        if (instance->mqttManager == nullptr || !instance->mqttManager->isConnected()) {
            ESP_LOGW(TAG, "MQTT disconnected during replay, pausing");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        // Dequeue packet from PSRAM
        const RawPacket* packet = instance->psramBuffer->dequeue();
        if (packet == nullptr) {
            break;
        }
        
        // Create a copy since dequeue returns pointer to internal buffer
        RawPacket packetCopy;
        memcpy(&packetCopy, packet, sizeof(RawPacket));
        
        // Publish the packet
        instance->publishRawBatch(&packetCopy);
        
        // Small delay to avoid flooding
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "Replay completed, replayed: %d packets", 
             instance->psramBuffer->getTotalReplayed());
    
    // Mark replay as done
    instance->replayInProgress = false;
    instance->replayTaskHandle = nullptr;
    
    // Delete self
    vTaskDelete(NULL);
}

uint32_t SignalTelemetry::getPSRAMBufferCount() const {
    return psramBuffer ? psramBuffer->getCount() : 0;
}

uint32_t SignalTelemetry::getPSRAMDroppedCount() const {
    return psramBuffer ? psramBuffer->getDroppedCount() : 0;
}

float SignalTelemetry::getPSRAMUsagePercent() const {
    return psramBuffer ? psramBuffer->getUsagePercent() : 0.0f;
}

bool SignalTelemetry::configureRMT(uint8_t pin, rmt_channel_handle_t* channel) {
    // RMT configuration using new ESP-IDF v5.x API
    // For simplicity, RMT is currently disabled - using ISR fallback
    // TODO: Implement new RMT RX API when pulse width measurement is needed
    ESP_LOGW(TAG, "RMT not implemented in this version, using ISR fallback");
    return false;
}

void SignalTelemetry::cleanupRMT(uint8_t pin) {
    // No cleanup needed as RMT is not implemented yet
}
