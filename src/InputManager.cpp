#include "InputManager.h"
#include "MQTTManager.h"

// Static member initialization
std::map<uint8_t, InputManager*> InputManager::isrHandlers;

InputManager::InputManager() 
    : mqttManager(nullptr), eventQueue(nullptr), workerTaskHandle(nullptr) {
}

InputManager::~InputManager() {
    // Cleanup interrupts
    for (auto& pair : configuredPins) {
        if (pair.second.mode == PinMode::INTERRUPT_MODE) {
            detachPinInterrupt(pair.first);
        }
    }
    
    // Delete task and queue
    if (workerTaskHandle != nullptr) {
        vTaskDelete(workerTaskHandle);
    }
    if (eventQueue != nullptr) {
        vQueueDelete(eventQueue);
    }
    
    preferences.end();
}

void InputManager::begin(MQTTManager* mqtt) {
    mqttManager = mqtt;
    
    // Initialize preferences
    preferences.begin("io", false);
    
    // Load exclude list
    loadExcludeList();
    
    // Create event queue
    eventQueue = xQueueCreate(QUEUE_SIZE, sizeof(IOEvent));
    if (eventQueue == nullptr) {
        Serial.println("ERROR: Failed to create event queue");
        return;
    }
    
    // Create worker task
    BaseType_t result = xTaskCreate(
        workerTaskFunction,
        "IOWorker",
        4096,              // Stack size
        this,              // Parameter (this pointer)
        5,                 // Priority
        &workerTaskHandle
    );
    
    if (result != pdPASS) {
        Serial.println("ERROR: Failed to create worker task");
        return;
    }
    
    // Load and apply saved configurations
    loadConfig();
    
    Serial.println("InputManager initialized");
}

void InputManager::loop() {
    // Check periodic reporting for configured pins
    unsigned long now = millis();
    
    for (auto& pair : configuredPins) {
        PinConfig& config = pair.second;
        
        // Skip if no report topic or no interval
        if (config.reportTopic.length() == 0 || config.reportIntervalMs == 0) {
            continue;
        }
        
        // Check if it's time to report
        if (now - config.lastReportTime >= config.reportIntervalMs) {
            config.lastReportTime = now;
            
            // Read current value
            int value;
            if (config.mode == PinMode::ANALOG_MODE) {
                value = analogRead(config.pin);
            } else {
                value = digitalRead(config.pin);
            }
            
            // Queue event for processing
            IOEvent event;
            event.pin = config.pin;
            event.type = (config.mode == PinMode::ANALOG_MODE) ? EventType::ANALOG_READ : EventType::DIGITAL;
            event.value = value;
            event.timestamp = now;
            queueEvent(event);
        }
    }
}

bool InputManager::configurePin(const JsonDocument& config) {
    // Extract and validate pin number
    if (!config.containsKey("pin")) {
        Serial.println("ERROR: Pin number not specified");
        return false;
    }
    
    uint8_t pin = config["pin"];
    
    if (!validatePin(pin)) {
        Serial.println("ERROR: Pin " + String(pin) + " is excluded or reserved");
        return false;
    }
    
    // Parse configuration
    PinConfig pinConfig;
    pinConfig.pin = pin;
    
    // Parse mode
    String modeStr = config["mode"] | "input";
    if (modeStr == "output") {
        pinConfig.mode = PinMode::OUTPUT_MODE;
    } else if (modeStr == "input") {
        pinConfig.mode = PinMode::INPUT_MODE;
    } else if (modeStr == "input_pullup") {
        pinConfig.mode = PinMode::INPUT_PULLUP_MODE;
    } else if (modeStr == "analog") {
        pinConfig.mode = PinMode::ANALOG_MODE;
    } else if (modeStr == "interrupt") {
        pinConfig.mode = PinMode::INTERRUPT_MODE;
    } else {
        Serial.println("ERROR: Invalid mode: " + modeStr);
        return false;
    }
    
    // Parse interrupt edge
    String edgeStr = config["edge"] | "change";
    if (edgeStr == "rising") {
        pinConfig.edge = InterruptEdge::RISING_EDGE;
    } else if (edgeStr == "falling") {
        pinConfig.edge = InterruptEdge::FALLING_EDGE;
    } else if (edgeStr == "change") {
        pinConfig.edge = InterruptEdge::CHANGE_EDGE;
    } else {
        pinConfig.edge = InterruptEdge::NONE;
    }
    
    // Parse other parameters
    pinConfig.debounceMs = config["debounce"] | 50;
    pinConfig.pulseWidthMs = config["pulse"] | 100;
    pinConfig.reportIntervalMs = config["interval"] | 0;
    pinConfig.reportTopic = config["report_topic"] | "";
    pinConfig.persist = config["persist"] | false;
    pinConfig.retain = config["retain"] | false;
    pinConfig.lastReportTime = 0;
    pinConfig.lastValue = -1;
    
    // Validate report_topic is required
    if (pinConfig.reportTopic.length() == 0) {
        Serial.println("ERROR: report_topic is required");
        return false;
    }
    
    // Remove old configuration if exists
    if (configuredPins.find(pin) != configuredPins.end()) {
        removePin(pin);
    }
    
    // Configure hardware
    configurePinHardware(pinConfig);
    
    // Store configuration
    configuredPins[pin] = pinConfig;
    
    // Save to NVS if persist is true
    if (pinConfig.persist) {
        saveConfig();
    }
    
    Serial.println("Pin " + String(pin) + " configured as " + modeStr);
    
    // Publish initial state
    if (pinConfig.mode != PinMode::OUTPUT_MODE) {
        int value;
        if (pinConfig.mode == PinMode::ANALOG_MODE) {
            value = analogRead(pin);
        } else {
            value = digitalRead(pin);
        }
        publishPinState(pin, value);
    }
    
    return true;
}

bool InputManager::removePin(uint8_t pin) {
    auto it = configuredPins.find(pin);
    if (it == configuredPins.end()) {
        return false;
    }
    
    // Detach interrupt if needed
    if (it->second.mode == PinMode::INTERRUPT_MODE) {
        detachPinInterrupt(pin);
    }
    
    // Remove from map
    configuredPins.erase(it);
    
    // Update NVS
    saveConfig();
    
    Serial.println("Pin " + String(pin) + " removed");
    return true;
}

bool InputManager::setExcludeList(const std::vector<uint8_t>& pins,
                                  const std::vector<std::pair<uint8_t, uint8_t>>& ranges,
                                  bool persist) {
    excludedPins = pins;
    excludedRanges = ranges;
    
    if (persist) {
        saveExcludeList(pins, ranges, true);
    }
    
    Serial.println("Exclude list updated with " + String(pins.size()) + 
                   " pins and " + String(ranges.size()) + " ranges");
    return true;
}

void InputManager::getExcludeList(std::vector<uint8_t>& pins,
                                  std::vector<std::pair<uint8_t, uint8_t>>& ranges) {
    pins = excludedPins;
    ranges = excludedRanges;
}

bool InputManager::triggerPin(uint8_t pin, const String& action, uint16_t pulseWidthMs) {
    auto it = configuredPins.find(pin);
    if (it == configuredPins.end()) {
        Serial.println("ERROR: Pin " + String(pin) + " not configured");
        return false;
    }
    
    if (it->second.mode != PinMode::OUTPUT_MODE) {
        Serial.println("ERROR: Pin " + String(pin) + " not configured as output");
        return false;
    }
    
    TriggerType type;
    if (action == "set") {
        type = TriggerType::SET;
    } else if (action == "reset") {
        type = TriggerType::RESET;
    } else if (action == "pulse") {
        type = TriggerType::PULSE;
        if (pulseWidthMs == 0) {
            pulseWidthMs = it->second.pulseWidthMs;
        }
    } else if (action == "toggle") {
        type = TriggerType::TOGGLE;
    } else {
        Serial.println("ERROR: Invalid action: " + action);
        return false;
    }
    
    applyTrigger(pin, type, pulseWidthMs);
    return true;
}

void InputManager::reportAllPins() {
    for (auto& pair : configuredPins) {
        PinConfig& config = pair.second;
        
        if (config.reportTopic.length() == 0) {
            continue;
        }
        
        int value;
        if (config.mode == PinMode::ANALOG_MODE) {
            value = analogRead(config.pin);
        } else {
            value = digitalRead(config.pin);
        }
        
        publishPinState(config.pin, value);
    }
}

String InputManager::getConfigJson() {
    StaticJsonDocument<2048> doc;
    JsonArray pinsArray = doc.createNestedArray("pins");
    
    for (auto& pair : configuredPins) {
        JsonObject pinObj = pinsArray.createNestedObject();
        PinConfig& config = pair.second;
        
        pinObj["pin"] = config.pin;
        
        String modeStr;
        switch (config.mode) {
            case PinMode::OUTPUT_MODE: modeStr = "output"; break;
            case PinMode::INPUT_MODE: modeStr = "input"; break;
            case PinMode::INPUT_PULLUP_MODE: modeStr = "input_pullup"; break;
            case PinMode::ANALOG_MODE: modeStr = "analog"; break;
            case PinMode::INTERRUPT_MODE: modeStr = "interrupt"; break;
            default: modeStr = "none"; break;
        }
        pinObj["mode"] = modeStr;
        
        pinObj["report_topic"] = config.reportTopic;
        pinObj["interval"] = config.reportIntervalMs;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// Private methods

void InputManager::loadConfig() {
    String configJson = preferences.getString("pins", "");
    if (configJson.length() == 0) {
        Serial.println("No saved pin configurations");
        return;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, configJson);
    
    if (error) {
        Serial.println("ERROR: Failed to parse saved config");
        return;
    }
    
    JsonArray pinsArray = doc["pins"];
    for (JsonVariant pinVariant : pinsArray) {
        StaticJsonDocument<512> pinDoc;
        JsonObject obj = pinVariant.as<JsonObject>();
        pinDoc.set(obj);
        configurePin(pinDoc);
    }
    
    Serial.println("Loaded " + String(configuredPins.size()) + " pin configurations");
}

void InputManager::saveConfig() {
    StaticJsonDocument<2048> doc;
    JsonArray pinsArray = doc.createNestedArray("pins");
    
    for (auto& pair : configuredPins) {
        if (!pair.second.persist) {
            continue;
        }
        
        JsonObject pinObj = pinsArray.createNestedObject();
        PinConfig& config = pair.second;
        
        pinObj["pin"] = config.pin;
        
        String modeStr;
        switch (config.mode) {
            case PinMode::OUTPUT_MODE: modeStr = "output"; break;
            case PinMode::INPUT_MODE: modeStr = "input"; break;
            case PinMode::INPUT_PULLUP_MODE: modeStr = "input_pullup"; break;
            case PinMode::ANALOG_MODE: modeStr = "analog"; break;
            case PinMode::INTERRUPT_MODE: modeStr = "interrupt"; break;
            default: continue;
        }
        pinObj["mode"] = modeStr;
        
        String edgeStr;
        switch (config.edge) {
            case InterruptEdge::RISING_EDGE: edgeStr = "rising"; break;
            case InterruptEdge::FALLING_EDGE: edgeStr = "falling"; break;
            case InterruptEdge::CHANGE_EDGE: edgeStr = "change"; break;
            default: edgeStr = "none"; break;
        }
        if (edgeStr != "none") {
            pinObj["edge"] = edgeStr;
        }
        
        pinObj["debounce"] = config.debounceMs;
        pinObj["pulse"] = config.pulseWidthMs;
        pinObj["interval"] = config.reportIntervalMs;
        pinObj["report_topic"] = config.reportTopic;
        pinObj["persist"] = config.persist;
        pinObj["retain"] = config.retain;
    }
    
    String output;
    serializeJson(doc, output);
    preferences.putString("pins", output);
    
    Serial.println("Configuration saved");
}

void InputManager::loadExcludeList() {
    String excludeJson = preferences.getString("exclude", "");
    if (excludeJson.length() == 0) {
        Serial.println("No saved exclude list");
        return;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, excludeJson);
    
    if (error) {
        Serial.println("ERROR: Failed to parse exclude list");
        return;
    }
    
    excludedPins.clear();
    excludedRanges.clear();
    
    JsonArray pinsArray = doc["pins"];
    for (JsonVariant pin : pinsArray) {
        excludedPins.push_back(pin.as<uint8_t>());
    }
    
    JsonArray rangesArray = doc["ranges"];
    for (JsonVariant rangeVariant : rangesArray) {
        JsonObject rangeObj = rangeVariant.as<JsonObject>();
        uint8_t from = rangeObj["from"];
        uint8_t to = rangeObj["to"];
        excludedRanges.push_back(std::make_pair(from, to));
    }
    
    Serial.println("Loaded exclude list: " + String(excludedPins.size()) + 
                   " pins, " + String(excludedRanges.size()) + " ranges");
}

void InputManager::saveExcludeList(const std::vector<uint8_t>& pins,
                                   const std::vector<std::pair<uint8_t, uint8_t>>& ranges,
                                   bool persist) {
    StaticJsonDocument<512> doc;
    
    JsonArray pinsArray = doc.createNestedArray("pins");
    for (uint8_t pin : pins) {
        pinsArray.add(pin);
    }
    
    JsonArray rangesArray = doc.createNestedArray("ranges");
    for (auto& range : ranges) {
        JsonObject rangeObj = rangesArray.createNestedObject();
        rangeObj["from"] = range.first;
        rangeObj["to"] = range.second;
    }
    
    String output;
    serializeJson(doc, output);
    preferences.putString("exclude", output);
    
    Serial.println("Exclude list saved");
}

bool InputManager::isPinExcluded(uint8_t pin) {
    // Check individual pins
    for (uint8_t excluded : excludedPins) {
        if (pin == excluded) {
            return true;
        }
    }
    
    // Check ranges
    for (auto& range : excludedRanges) {
        if (pin >= range.first && pin <= range.second) {
            return true;
        }
    }
    
    return false;
}

bool InputManager::isPinReserved(uint8_t pin) {
    for (uint8_t reserved : RESERVED_PINS) {
        if (pin == reserved) {
            return true;
        }
    }
    return false;
}

bool InputManager::validatePin(uint8_t pin) {
    if (isPinReserved(pin)) {
        Serial.println("ERROR: Pin " + String(pin) + " is reserved");
        return false;
    }
    
    if (isPinExcluded(pin)) {
        Serial.println("ERROR: Pin " + String(pin) + " is excluded");
        return false;
    }
    
    return true;
}

void InputManager::configurePinHardware(const PinConfig& config) {
    switch (config.mode) {
        case PinMode::OUTPUT_MODE:
            pinMode(config.pin, OUTPUT);
            digitalWrite(config.pin, LOW);
            break;
            
        case PinMode::INPUT_MODE:
            pinMode(config.pin, INPUT);
            break;
            
        case PinMode::INPUT_PULLUP_MODE:
            pinMode(config.pin, INPUT_PULLUP);
            break;
            
        case PinMode::ANALOG_MODE:
            // Analog pins don't need pinMode on ESP32
            break;
            
        case PinMode::INTERRUPT_MODE:
            pinMode(config.pin, INPUT_PULLUP);
            attachPinInterrupt(config.pin, config.edge);
            break;
            
        default:
            break;
    }
}

void InputManager::attachPinInterrupt(uint8_t pin, InterruptEdge edge) {
    // Store handler mapping
    isrHandlers[pin] = this;
    
    int mode;
    switch (edge) {
        case InterruptEdge::RISING_EDGE:
            mode = RISING;
            break;
        case InterruptEdge::FALLING_EDGE:
            mode = FALLING;
            break;
        case InterruptEdge::CHANGE_EDGE:
            mode = CHANGE;
            break;
        default:
            return;
    }
    
    // Attach interrupt with argument
    attachInterruptArg(digitalPinToInterrupt(pin), handleInterrupt, (void*)(uintptr_t)pin, mode);
}

void InputManager::detachPinInterrupt(uint8_t pin) {
    detachInterrupt(digitalPinToInterrupt(pin));
    isrHandlers.erase(pin);
}

void IRAM_ATTR InputManager::handleInterrupt(void* arg) {
    uint8_t pin = (uint8_t)(uintptr_t)arg;
    
    auto it = isrHandlers.find(pin);
    if (it == isrHandlers.end()) {
        return;
    }
    
    InputManager* instance = it->second;
    
    // Create event
    IOEvent event;
    event.pin = pin;
    event.type = EventType::DIGITAL;
    event.value = digitalRead(pin);
    event.timestamp = millis();
    
    // Try to queue event
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // If queue is full, remove oldest item and add new one
    if (uxQueueSpacesAvailable(instance->eventQueue) == 0) {
        IOEvent dummy;
        xQueueReceiveFromISR(instance->eventQueue, &dummy, &xHigherPriorityTaskWoken);
    }
    
    xQueueSendFromISR(instance->eventQueue, &event, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void InputManager::workerTaskFunction(void* parameter) {
    InputManager* instance = static_cast<InputManager*>(parameter);
    IOEvent event;
    
    while (true) {
        // Wait for event (block indefinitely)
        if (xQueueReceive(instance->eventQueue, &event, portMAX_DELAY) == pdTRUE) {
            instance->processEvent(event);
        }
    }
}

void InputManager::processEvent(const IOEvent& event) {
    auto it = configuredPins.find(event.pin);
    if (it == configuredPins.end()) {
        return;
    }
    
    PinConfig& config = it->second;
    
    // Apply debounce
    unsigned long now = millis();
    if (config.debounceMs > 0) {
        if (now - config.lastReportTime < config.debounceMs) {
            return; // Too soon, ignore
        }
    }
    
    // Check if value changed (for on-change reporting)
    if (config.lastValue == event.value && config.reportIntervalMs == 0) {
        return; // No change, skip reporting
    }
    
    config.lastValue = event.value;
    config.lastReportTime = now;
    
    // Publish state
    publishPinState(event.pin, event.value);
}

void InputManager::applyTrigger(uint8_t pin, TriggerType type, uint16_t pulseWidthMs) {
    switch (type) {
        case TriggerType::SET:
            digitalWrite(pin, HIGH);
            publishPinState(pin, HIGH);
            break;
            
        case TriggerType::RESET:
            digitalWrite(pin, LOW);
            publishPinState(pin, LOW);
            break;
            
        case TriggerType::PULSE:
            digitalWrite(pin, HIGH);
            publishPinState(pin, HIGH);
            delay(pulseWidthMs);
            digitalWrite(pin, LOW);
            publishPinState(pin, LOW);
            break;
            
        case TriggerType::TOGGLE: {
            int currentState = digitalRead(pin);
            int newState = !currentState;
            digitalWrite(pin, newState);
            publishPinState(pin, newState);
            break;
        }
            
        default:
            break;
    }
}

void InputManager::publishPinState(uint8_t pin, int value) {
    auto it = configuredPins.find(pin);
    if (it == configuredPins.end() || mqttManager == nullptr) {
        return;
    }
    
    PinConfig& config = it->second;
    
    if (config.reportTopic.length() == 0) {
        return;
    }
    
    // Publish state
    mqttManager->publish(config.reportTopic, String(value), config.retain);
}

bool InputManager::queueEvent(const IOEvent& event) {
    if (eventQueue == nullptr) {
        return false;
    }
    
    // If queue is full, remove oldest and add new
    if (uxQueueSpacesAvailable(eventQueue) == 0) {
        IOEvent dummy;
        xQueueReceive(eventQueue, &dummy, 0);
    }
    
    return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}
