#include "SignalTelemetry.h"
#include "MQTTManager.h"
#include "esp_log.h"

static const char *TAG = "SignalTelemetry";

// Static member initialization
std::map<uint8_t, SignalTelemetry*> SignalTelemetry::isrHandlers;

SignalTelemetry::SignalTelemetry() 
    : mqttManager(nullptr), seq(0), droppedRaw(0), droppedPulse(0), rmtOverflow(0),
      signalRingBuffer(nullptr), batchQueue(nullptr), 
      collectTaskHandle(nullptr), publishTaskHandle(nullptr) {
}

SignalTelemetry::~SignalTelemetry() {
    // Cleanup interrupts and RMT
    for (auto& pair : configuredPins) {
        detachInterrupt(digitalPinToInterrupt(pair.first));
        if (pair.second.useRMT) {
            cleanupRMT(pair.first);
        }
    }
    
    // Delete tasks
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
}

void SignalTelemetry::begin(MQTTManager* mqtt, const std::string& macAddr) {
    mqttManager = mqtt;
    macAddress = macAddr;
    
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
    
    // Configure hardware
    pinMode(pin, INPUT);
    
    // Setup RMT if requested and capturing pulse
    if (capturePulse && useRMT) {
        // Try to allocate RMT channel (0-7 available on ESP32)
        // For simplicity, use pin number modulo 8 as channel
        rmt_channel_t channel = (rmt_channel_t)(pin % 8);
        if (configureRMT(pin, channel)) {
            config.rmtChannel = channel;
        } else {
            ESP_LOGW(TAG, "WARNING: RMT configuration failed for pin %u, using ISR fallback", pin);
            config.useRMT = false;
        }
    }
    
    // Attach interrupt for raw edge capture (always attach if captureRaw or capturePulse without RMT)
    if (captureRaw || (capturePulse && !config.useRMT)) {
        isrHandlers[pin] = this;
        attachInterruptArg(digitalPinToInterrupt(pin), edgeISR, (void*)(uintptr_t)pin, CHANGE);
    }
    
    configuredPins[pin] = config;
    
    ESP_LOGI(TAG, "Pin %u configured for signal telemetry", pin);
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
    detachInterrupt(digitalPinToInterrupt(pin));
    isrHandlers.erase(pin);
    
    // Cleanup RMT if used
    if (it->second.useRMT) {
        cleanupRMT(pin);
    }
    
    configuredPins.erase(it);
    
    ESP_LOGI(TAG, "Pin %u removed from signal telemetry", pin);
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
    uint8_t value = digitalRead(pin);
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
            instance->publishRawBatch(&batch);
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
    ESP_LOGI(TAG, "  Dropped Raw: %u", diag.droppedRaw);
    ESP_LOGI(TAG, "  Dropped Pulse: %u", diag.droppedPulse);
    ESP_LOGI(TAG, "  Queue Depth: %u", diag.queueDepth);
    ESP_LOGI(TAG, "  RMT Overflow: %u", diag.rmtOverflow);
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
    }
    
    // Start receiving
    err = rmt_rx_start(channel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ERROR: RMT rx start failed: 0x%x", err);
        rmt_driver_uninstall(channel);
        return false;
    }
    
    return true;
}

void SignalTelemetry::cleanupRMT(uint8_t pin) {
    auto it = configuredPins.find(pin);
    if (it != configuredPins.end() && it->second.useRMT) {
        rmt_rx_stop(it->second.rmtChannel);
        rmt_driver_uninstall(it->second.rmtChannel);
    }
}
