#ifndef SIGNAL_TELEMETRY_H
#define SIGNAL_TELEMETRY_H

#include <stdint.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "esp_timer.h"
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include <map>
#include <vector>

// Forward declarations
class MQTTManager;
class PSRAMBufferManager;

// Binary packet structures
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
    PacketHeader header;
    uint64_t baseTimeUs;
    uint32_t baseSeq;
    uint8_t  count;
    RawEdge  edges[50]; // MAX_BATCH
};

struct PulsePacket {
    PacketHeader header;
    uint8_t  pinId;
    uint32_t highUs;
    uint32_t lowUs;
    uint64_t deviceTimeUs;
    uint32_t seq;
};

struct DiagPacket {
    PacketHeader header;
    uint32_t droppedRaw;
    uint32_t droppedPulse;
    uint16_t queueDepth;
    uint16_t rmtOverflow;
};

#pragma pack(pop)

// Event types for lock-free ring buffer
enum class SignalEventType {
    EDGE_CHANGE,
    PULSE_WIDTH
};

struct SignalEvent {
    SignalEventType type;
    uint8_t pinId;
    uint8_t value;
    uint64_t deviceTimeUs;
    uint32_t highUs;
    uint32_t lowUs;
};

// Pin configuration for signal capture
struct SignalPinConfig {
    uint8_t pin;
    bool captureRaw;       // Capture raw edge changes
    bool capturePulse;     // Capture pulse width
    bool useRMT;           // Use RMT for pulse measurement
    rmt_channel_handle_t rmtChannel;
    std::string rawTopic;       // Topic for raw edge data
    std::string pulseTopic;     // Topic for pulse width data
};

class SignalTelemetry {
private:
    MQTTManager* mqttManager;
    PSRAMBufferManager* psramBuffer;
    std::string macAddress;
    
    // Sequence counter (monotonic, reset on reboot)
    uint32_t seq;
    
    // Diagnostic counters
    uint32_t droppedRaw;
    uint32_t droppedPulse;
    uint16_t rmtOverflow;
    
    // MQTT connection state tracking
    enum class MQTTState {
        DISCONNECTED,
        CONNECTED_FAST,
        CONNECTED_SLOW
    };
    MQTTState mqttState;
    unsigned long lastPublishSuccess;
    unsigned long lastPublishAttempt;
    static const unsigned long MQTT_SLOW_THRESHOLD_MS = 5000;  // If publish takes >5s, consider it slow
    
    // Replay state
    bool replayInProgress;
    TaskHandle_t replayTaskHandle;
    
    // Pin configurations
    std::map<uint8_t, SignalPinConfig> configuredPins;
    
    // FreeRTOS components
    RingbufHandle_t signalRingBuffer;
    QueueHandle_t batchQueue;
    TaskHandle_t collectTaskHandle;
    TaskHandle_t publishTaskHandle;
    
    // Batching parameters
    static const uint8_t MAX_BATCH_SIZE = 50;
    static const uint32_t MAX_BATCH_TIME_MS = 50;
    static const size_t RING_BUFFER_SIZE = 4096;
    static const uint8_t BATCH_QUEUE_SIZE = 10;
    
    // ISR handlers map
    static std::map<uint8_t, SignalTelemetry*> isrHandlers;
    
    // Internal methods
    static void IRAM_ATTR edgeISR(void* arg);
    static void collectTaskFunction(void* parameter);
    static void publishTaskFunction(void* parameter);
    static void replayTaskFunction(void* parameter);
    
    void processBatch(RawPacket* batch);
    void publishRawBatch(const RawPacket* batch);
    void publishPulse(const PulsePacket* pulse);
    void publishDiagnostics();
    
    bool queueSignalEvent(const SignalEvent& event);
    uint32_t getNextSeq();
    
    // MQTT state management
    void updateMQTTState();
    bool shouldUseDirectPublish() const;
    bool tryDirectPublish(const RawPacket* batch);
    void spillToPSRAM(const RawPacket* batch);
    
    // Replay management
    void startReplay();
    void stopReplay();
    
    // RMT configuration for pulse width measurement
    bool configureRMT(uint8_t pin, rmt_channel_handle_t* channel);
    void cleanupRMT(uint8_t pin);
    
public:
    SignalTelemetry();
    ~SignalTelemetry();
    
    void begin(MQTTManager* mqtt, const std::string& macAddr);
    void loop();
    
    // Pin configuration
    bool configurePin(uint8_t pin, bool captureRaw, bool capturePulse, bool useRMT = false);
    bool removePin(uint8_t pin);
    
    // Get diagnostics
    DiagPacket getDiagnostics();
    
    // Get PSRAM buffer stats
    uint32_t getPSRAMBufferCount() const;
    uint32_t getPSRAMDroppedCount() const;
    float getPSRAMUsagePercent() const;
    
    // Public methods for publishing
    void publishHeartbeat();
    
    // Boot behavior
    void onBoot();
};

#endif // SIGNAL_TELEMETRY_H
