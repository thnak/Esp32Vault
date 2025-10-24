#include <Arduino.h>
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "OTAManager.h"
#include "InputManager.h"
#include <ArduinoJson.h>

// Manager instances
WiFiManager wifiManager;
MQTTManager mqttManager;
OTAManager otaManager;
InputManager inputManager;

// Status variables
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_INTERVAL = 30000; // 30 seconds

// Signal strength variables
unsigned long lastSignalUpdate = 0;
const unsigned long SIGNAL_INTERVAL = 10000; // 10 seconds

void handleMQTTMessage(String topic, String payload);
void publishDeviceInfo();
void publishSignalStrength();
void handleConfigCommand(const String& payload);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("ESP32 Vault Starting...");
    Serial.println("=================================\n");
    
    // Initialize WiFi Manager
    Serial.println("Initializing WiFi...");
    wifiManager.begin();
    
    // Initialize MQTT Manager if WiFi is connected
    if (wifiManager.isConnected()) {
        Serial.println("Initializing MQTT...");
        mqttManager.begin();
        mqttManager.setCallback(handleMQTTMessage);
        
        // Initialize OTA
        Serial.println("Initializing OTA...");
        otaManager.begin();
        
        // Initialize Input Manager
        Serial.println("Initializing Input Manager...");
        inputManager.begin(&mqttManager);
    }
    
    Serial.println("\n=================================");
    Serial.println("Setup Complete!");
    Serial.println("=================================\n");
}

void loop() {
    // Handle WiFi Manager
    wifiManager.loop();
    
    // Only run MQTT and OTA if WiFi is connected and not in AP mode
    if (wifiManager.isConnected() && !wifiManager.isAPMode()) {
        mqttManager.loop();
        otaManager.loop();
        inputManager.loop();
        
        // Periodic status update
        unsigned long now = millis();
        if (now - lastStatusUpdate > STATUS_INTERVAL) {
            lastStatusUpdate = now;
            publishDeviceInfo();
        }
        
        // Periodic signal strength update
        if (now - lastSignalUpdate > SIGNAL_INTERVAL) {
            lastSignalUpdate = now;
            publishSignalStrength();
        }
    }
    
    delay(10);
}

void handleMQTTMessage(String topic, String payload) {
    Serial.print("Processing message: ");
    Serial.print(topic);
    Serial.print(" = ");
    Serial.println(payload);
    
    // Handle configuration updates
    if (topic.endsWith("/config/set")) {
        handleConfigCommand(payload);
    }
    // Handle MQTT broker configuration
    else if (topic.endsWith("/cmd/mqtt")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            String server = doc["server"] | "";
            int port = doc["port"] | 1883;
            String user = doc["user"] | "";
            String password = doc["password"] | "";
            
            if (server.length() > 0) {
                mqttManager.saveConfig(server, port, user, password);
                mqttManager.publishStatus("mqtt_config_updated");
                Serial.println("MQTT configuration updated via MQTT");
            }
        }
    }
    // Handle OTA trigger
    else if (topic.endsWith("/cmd/ota")) {
        if (payload == "enable") {
            if (!otaManager.isEnabled()) {
                otaManager.begin();
            }
            mqttManager.publishStatus("ota_enabled");
        }
    }
    // Handle restart command
    else if (topic.endsWith("/cmd/restart")) {
        mqttManager.publishStatus("restarting");
        delay(1000);
        ESP.restart();
    }
    // Handle WiFi reset command
    else if (topic.endsWith("/cmd/reset_wifi")) {
        wifiManager.clearCredentials();
        mqttManager.publishStatus("wifi_reset");
        delay(1000);
        ESP.restart();
    }
    // Handle IO configuration
    else if (topic.endsWith("/cmd/io/config")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            if (inputManager.configurePin(doc)) {
                mqttManager.publishStatus("io_config_updated");
                Serial.println("IO configuration updated via MQTT");
            } else {
                mqttManager.publishStatus("io_config_failed");
                Serial.println("IO configuration failed");
            }
        }
    }
    // Handle IO exclude list
    else if (topic.endsWith("/cmd/io/exclude")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            std::vector<uint8_t> pins;
            std::vector<std::pair<uint8_t, uint8_t>> ranges;
            bool persist = doc["persist"] | false;
            
            JsonArray pinsArray = doc["pins"];
            for (JsonVariant pin : pinsArray) {
                pins.push_back(pin.as<uint8_t>());
            }
            
            JsonArray rangesArray = doc["ranges"];
            for (JsonVariant rangeVariant : rangesArray) {
                JsonObject rangeObj = rangeVariant.as<JsonObject>();
                uint8_t from = rangeObj["from"];
                uint8_t to = rangeObj["to"];
                ranges.push_back(std::make_pair(from, to));
            }
            
            if (inputManager.setExcludeList(pins, ranges, persist)) {
                mqttManager.publishStatus("io_exclude_updated");
                Serial.println("IO exclude list updated via MQTT");
            }
        }
    }
    // Handle IO trigger - match pattern /cmd/io/{pin}/trigger
    else if (topic.indexOf("/cmd/io/") >= 0 && topic.endsWith("/trigger")) {
        // Extract pin number from topic
        int startIdx = topic.indexOf("/cmd/io/") + 8;
        int endIdx = topic.indexOf("/trigger");
        String pinStr = topic.substring(startIdx, endIdx);
        uint8_t pin = pinStr.toInt();
        
        // Parse payload for action
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        String action;
        uint16_t pulseWidth = 100;
        
        if (!error && doc.is<JsonObject>()) {
            action = doc["action"] | "set";
            pulseWidth = doc["pulse"] | 100;
        } else {
            // Payload is plain text action
            action = payload;
        }
        
        if (inputManager.triggerPin(pin, action, pulseWidth)) {
            mqttManager.publishStatus("io_trigger_success");
            Serial.println("IO trigger executed on pin " + String(pin));
        } else {
            mqttManager.publishStatus("io_trigger_failed");
            Serial.println("IO trigger failed on pin " + String(pin));
        }
    }
}

void publishDeviceInfo() {
    if (!mqttManager.isConnected()) {
        return;
    }
    
    StaticJsonDocument<512> doc;
    
    // Device information
    doc["device_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mqtt_connected"] = mqttManager.isConnected();
    doc["ota_enabled"] = otaManager.isEnabled();
    
    String output;
    serializeJson(doc, output);
    
    mqttManager.publishStatus(output);
}

void publishSignalStrength() {
    if (!mqttManager.isConnected()) {
        return;
    }
    
    int rssi = WiFi.RSSI();
    mqttManager.publishSignalStrength(rssi);
}

void handleConfigCommand(const String& payload) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
        // Handle different configuration parameters
        if (doc.containsKey("status_interval")) {
            // Could be used to adjust status update interval
            Serial.println("Configuration updated");
        }
        
        mqttManager.publishStatus("config_updated");
    } else {
        Serial.println("Failed to parse config JSON");
    }
}
