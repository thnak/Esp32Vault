#include <Arduino.h>
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "OTAManager.h"
#include "SignalTelemetry.h"
#include <ArduinoJson.h>

// Manager instances
WiFiManager wifiManager;
MQTTManager mqttManager;
OTAManager otaManager;
SignalTelemetry signalTelemetry;

// Status variables
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_INTERVAL = 30000; // 30 seconds

// Heartbeat interval
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 seconds

void handleMQTTMessage(String topic, String payload);
void publishDeviceInfo();
void publishOTAStatus(const String& status);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("ESP32 Vault - Signal Telemetry v1");
    Serial.println("=================================\n");
    
    // Initialize WiFi Manager
    Serial.println("Initializing WiFi...");
    wifiManager.begin();
    
    // Initialize MQTT Manager if WiFi is connected
    if (wifiManager.isConnected()) {
        Serial.println("Initializing MQTT...");
        mqttManager.begin();
        mqttManager.setCallback(handleMQTTMessage);
        
        // Initialize OTA with callback for status publishing
        Serial.println("Initializing OTA...");
        String deviceId = mqttManager.getMacAddress();
        otaManager.begin(deviceId);
        otaManager.setStatusCallback(publishOTAStatus);
        
        // Initialize Signal Telemetry
        Serial.println("Initializing Signal Telemetry...");
        signalTelemetry.begin(&mqttManager, mqttManager.getMacAddress());
        signalTelemetry.onBoot();
    }
    
    Serial.println("\n=================================");
    Serial.println("Setup Complete!");
    Serial.println("Firmware: Signal Telemetry v1");
    Serial.println("Philosophy: Firmware = Oscilloscope, Server = Judge");
    Serial.println("=================================\n");
}

void loop() {
    // Handle WiFi Manager (includes 1ms delay internally)
    wifiManager.loop();
    
    // Only run MQTT if WiFi is connected
    if (wifiManager.isConnected()) {
        mqttManager.loop();
        otaManager.loop();
        signalTelemetry.loop();
        
        // Periodic status update
        unsigned long now = millis();
        if (now - lastStatusUpdate > STATUS_INTERVAL) {
            lastStatusUpdate = now;
            publishDeviceInfo();
        }
        
        // Periodic heartbeat
        if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
            lastHeartbeat = now;
            signalTelemetry.publishHeartbeat();
        }
        
        // Additional delay for normal operation (total ~10ms with WiFiManager delay)
        delay(9);
    } else {
        // Not connected, use normal delay
        delay(9);
    }
}

void handleMQTTMessage(String topic, String payload) {
    Serial.print("Processing message: ");
    Serial.print(topic);
    Serial.print(" = ");
    Serial.println(payload);
    
    // Handle MQTT broker configuration
    if (topic.endsWith("/cmd/mqtt")) {
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
    // Handle OTA update command
    else if (topic.endsWith("/cmd/ota_update")) {
        otaManager.handleUpdateCommand(payload);
    }
    // Handle restart command
    else if (topic.endsWith("/cmd/restart")) {
        mqttManager.publishStatus("restarting");
        delay(1000);
        ESP.restart();
    }
    // Handle WiFi credential update command
    else if (topic.endsWith("/cmd/wifi")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            String newSSID = doc["ssid"] | "";
            String newPassword = doc["password"] | "";
            
            if (newSSID.length() > 0 && newPassword.length() > 0) {
                wifiManager.saveCredentials(newSSID, newPassword);
                mqttManager.publishStatus("wifi_credentials_updated");
                Serial.println("WiFi credentials updated via MQTT");
                delay(1000);
                ESP.restart();
            } else {
                mqttManager.publishStatus("wifi_update_failed_invalid_params");
                Serial.println("Invalid WiFi credentials provided");
            }
        }
    }
    // Handle WiFi reset command
    else if (topic.endsWith("/cmd/reset_wifi")) {
        wifiManager.clearCredentials();
        mqttManager.publishStatus("wifi_reset");
        delay(1000);
        ESP.restart();
    }
    // Handle signal pin configuration
    else if (topic.endsWith("/cmd/signal/config")) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            uint8_t pin = doc["pin"] | 0;
            bool captureRaw = doc["capture_raw"] | true;
            bool capturePulse = doc["capture_pulse"] | false;
            bool useRMT = doc["use_rmt"] | false;
            
            if (signalTelemetry.configurePin(pin, captureRaw, capturePulse, useRMT)) {
                mqttManager.publishStatus("signal_pin_configured");
                Serial.println("Signal pin " + String(pin) + " configured");
            } else {
                mqttManager.publishStatus("signal_pin_config_failed");
                Serial.println("Signal pin configuration failed");
            }
        }
    }
    // Handle signal pin removal
    else if (topic.endsWith("/cmd/signal/remove")) {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            uint8_t pin = doc["pin"] | 0;
            
            if (signalTelemetry.removePin(pin)) {
                mqttManager.publishStatus("signal_pin_removed");
                Serial.println("Signal pin " + String(pin) + " removed");
            } else {
                mqttManager.publishStatus("signal_pin_remove_failed");
                Serial.println("Signal pin removal failed");
            }
        }
    }
}

void publishDeviceInfo() {
    if (!mqttManager.isConnected()) {
        return;
    }
    
    StaticJsonDocument<512> doc;
    
    // Device information
    doc["device_id"] = mqttManager.getMacAddress();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mqtt_connected"] = mqttManager.isConnected();
    doc["ota_update_in_progress"] = otaManager.isUpdateInProgress();
    doc["firmware_version"] = "Signal Telemetry v1";
    
    // Add diagnostics
    DiagPacket diag = signalTelemetry.getDiagnostics();
    doc["dropped_raw"] = diag.droppedRaw;
    doc["dropped_pulse"] = diag.droppedPulse;
    doc["queue_depth"] = diag.queueDepth;
    
    String output;
    serializeJson(doc, output);
    
    mqttManager.publishStatus(output);
}

void publishOTAStatus(const String& status) {
    if (mqttManager.isConnected()) {
        String deviceId = mqttManager.getMacAddress();
        String topic = "esp32vault/" + deviceId + "/ota/status";
        mqttManager.publish(topic, status);
    }
}
