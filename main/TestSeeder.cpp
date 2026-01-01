#include "TestSeeder.h"
#include "SignalTelemetry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char *TAG = "TestSeeder";

TestSeeder::TestSeeder() 
    : mqttManager(nullptr), seederTaskHandle(nullptr), seq(0), iteration(0) {
}

TestSeeder::~TestSeeder() {
    stop();
}

void TestSeeder::begin(MQTTManager* mqtt, const std::string& mac) {
    mqttManager = mqtt;
    macAddress = mac;
    
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "TEST SEEDER ENABLED");
    ESP_LOGI(TAG, "Generating synthetic telemetry every 1 second");
    ESP_LOGI(TAG, "WARNING: This is a test build!");
    ESP_LOGI(TAG, "===========================================");
    
    // Create seeder task
    BaseType_t result = xTaskCreate(
        seederTaskFunction,
        "TestSeeder",
        4096,
        this,
        5,  // Medium priority
        &seederTaskHandle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create test seeder task");
    }
}

void TestSeeder::stop() {
    if (seederTaskHandle != nullptr) {
        vTaskDelete(seederTaskHandle);
        seederTaskHandle = nullptr;
    }
}

void TestSeeder::seederTaskFunction(void* parameter) {
    TestSeeder* instance = static_cast<TestSeeder*>(parameter);
    
    ESP_LOGI(TAG, "Test seeder task started");
    
    while (true) {
        // Wait 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Generate and publish test data
        instance->generateAndPublishData();
        instance->iteration++;
    }
}

void TestSeeder::generateAndPublishData() {
    if (!mqttManager) {
        ESP_LOGW(TAG, "MQTT manager not initialized, skipping iteration %lu", iteration);
        return;
    }
    
    // Always attempt to publish - MQTT client will queue messages if not connected
    ESP_LOGI(TAG, "Seeding test data - iteration %lu (MQTT connected: %d)", 
             iteration, mqttManager->isConnected());
    
    // Publish all telemetry types
    publishRawPacket();
    publishPulsePacket();
    publishDiagPacket();
}

void TestSeeder::publishRawPacket() {
    RawPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Header
    packet.header.version = 1;
    packet.header.type = 1; // Raw
    
    // Timing
    packet.baseTimeUs = esp_timer_get_time();
    packet.baseSeq = seq++;
    
    // Generate synthetic edge data (5-10 edges per packet)
    uint8_t numEdges = 5 + (iteration % 6);
    packet.count = numEdges;
    
    uint32_t dtAccum = 0;
    for (uint8_t i = 0; i < numEdges && i < 50; i++) {
        packet.edges[i].pinId = 14;  // Test pin 14
        packet.edges[i].value = (i % 2);  // Alternating high/low
        
        // Variable timing: 50-200us between edges
        dtAccum += 50 + ((iteration + i) % 150);
        packet.edges[i].dtUs = dtAccum;
    }
    
    // Calculate payload size
    size_t payloadSize = sizeof(PacketHeader) + sizeof(uint64_t) + 
                        sizeof(uint32_t) + sizeof(uint8_t) + 
                        (packet.count * sizeof(RawEdge));
    
    // Publish to raw topic
    std::string topic = "esp32vault/raw/14";
    mqttManager->publishBinary(topic, (const uint8_t*)&packet, payloadSize,
                               CONTENT_TYPE_RAW_SIGNAL, false, 
                               MESSAGE_EXPIRY_TELEMETRY_SECONDS);
    
    ESP_LOGD(TAG, "Published raw packet: %d edges, seq=%u", packet.count, (unsigned)packet.baseSeq);
}

void TestSeeder::publishPulsePacket() {
    PulsePacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Header
    packet.header.version = 1;
    packet.header.type = 2; // Pulse
    
    // Pin
    packet.pinId = 15;  // Test pin 15
    
    // Generate varying pulse widths based on iteration
    // High: 100-500us, Low: 50-300us
    packet.highUs = 100 + ((iteration * 37) % 400);
    packet.lowUs = 50 + ((iteration * 23) % 250);
    
    // Timing
    packet.deviceTimeUs = esp_timer_get_time();
    packet.seq = seq++;
    
    // Publish to pulse topic
    std::string topic = "esp32vault/pulse/15";
    mqttManager->publishBinary(topic, (const uint8_t*)&packet, sizeof(PulsePacket),
                               CONTENT_TYPE_PULSE_SIGNAL, false,
                               MESSAGE_EXPIRY_TELEMETRY_SECONDS);
    
    ESP_LOGD(TAG, "Published pulse packet: high=%uus, low=%uus, seq=%u", 
             (unsigned)packet.highUs, (unsigned)packet.lowUs, (unsigned)packet.seq);
}

void TestSeeder::publishDiagPacket() {
    DiagPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Header
    packet.header.version = 1;
    packet.header.type = 3; // Diagnostic
    
    // Synthetic diagnostic data
    packet.droppedRaw = 0;
    packet.droppedPulse = 0;
    packet.queueDepth = iteration % 10;  // Simulated queue depth 0-9
    packet.rmtOverflow = 0;
    
    // Publish to diag topic
    mqttManager->publishBinary("esp32vault/diag", (const uint8_t*)&packet, 
                               sizeof(DiagPacket), CONTENT_TYPE_DIAG_SIGNAL, 
                               false, MESSAGE_EXPIRY_TELEMETRY_SECONDS);
    
    ESP_LOGD(TAG, "Published diag packet: queueDepth=%d", packet.queueDepth);
}
