#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <vector>
#include <map>

// Forward declaration
class MQTTManager;

// Pin modes
enum class PinMode {
    NONE,
    OUTPUT_MODE,
    INPUT_MODE,
    INPUT_PULLUP_MODE,
    ANALOG_MODE,
    INTERRUPT_MODE
};

// Trigger types
enum class TriggerType {
    NONE,
    SET,           // Set pin to HIGH
    RESET,         // Set pin to LOW
    PULSE,         // Pulse pin (HIGH then LOW)
    TOGGLE         // Toggle pin state
};

// Interrupt edge detection
enum class InterruptEdge {
    NONE,
    RISING_EDGE,
    FALLING_EDGE,
    CHANGE_EDGE
};

// Event types for the queue
enum class EventType {
    DIGITAL,
    ANALOG_READ,
    TRIGGER
};

// Event structure for ISR-safe queue
struct IOEvent {
    uint8_t pin;
    EventType type;
    int value;
    unsigned long timestamp;
};

// Pin configuration structure
struct PinConfig {
    uint8_t pin;
    PinMode mode;
    InterruptEdge edge;
    uint16_t debounceMs;
    uint16_t pulseWidthMs;
    uint32_t reportIntervalMs;
    String reportTopic;
    bool persist;
    bool retain;
    unsigned long lastReportTime;
    int lastValue;
};

class InputManager {
private:
    Preferences preferences;
    MQTTManager* mqttManager;
    
    // Pin management
    std::map<uint8_t, PinConfig> configuredPins;
    std::vector<uint8_t> excludedPins;
    std::vector<std::pair<uint8_t, uint8_t>> excludedRanges;
    
    // Default reserved pins (boot, flash, etc.)
    const std::vector<uint8_t> RESERVED_PINS = {
        6, 7, 8, 9, 10, 11  // SPI flash pins
    };
    
    // FreeRTOS event queue and task
    QueueHandle_t eventQueue;
    TaskHandle_t workerTaskHandle;
    static const uint8_t QUEUE_SIZE = 32;
    static const uint8_t QUEUE_OVERWRITE_OLDEST = 1;
    
    // ISR handlers map
    static std::map<uint8_t, InputManager*> isrHandlers;
    
    // Internal methods
    void loadConfig();
    void saveConfig();
    void loadExcludeList();
    void saveExcludeList(const std::vector<uint8_t>& pins, 
                        const std::vector<std::pair<uint8_t, uint8_t>>& ranges, 
                        bool persist);
    
    bool isPinExcluded(uint8_t pin);
    bool isPinReserved(uint8_t pin);
    bool validatePin(uint8_t pin);
    
    void configurePinHardware(const PinConfig& config);
    void attachPinInterrupt(uint8_t pin, InterruptEdge edge);
    void detachPinInterrupt(uint8_t pin);
    
    static void IRAM_ATTR handleInterrupt(void* arg);
    static void workerTaskFunction(void* parameter);
    
    void processEvent(const IOEvent& event);
    void applyTrigger(uint8_t pin, TriggerType type, uint16_t pulseWidthMs);
    void publishPinState(uint8_t pin, int value);
    
    bool queueEvent(const IOEvent& event);
    
public:
    InputManager();
    ~InputManager();
    
    void begin(MQTTManager* mqtt);
    void loop();
    
    // Pin configuration
    bool configurePin(const JsonDocument& config);
    bool removePin(uint8_t pin);
    
    // Exclude list management
    bool setExcludeList(const std::vector<uint8_t>& pins,
                       const std::vector<std::pair<uint8_t, uint8_t>>& ranges,
                       bool persist);
    void getExcludeList(std::vector<uint8_t>& pins,
                       std::vector<std::pair<uint8_t, uint8_t>>& ranges);
    
    // Trigger operations
    bool triggerPin(uint8_t pin, const String& action, uint16_t pulseWidthMs = 100);
    
    // Status reporting
    void reportAllPins();
    String getConfigJson();
};

#endif // INPUT_MANAGER_H
