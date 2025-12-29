/**
 * ESP32 Vault - Signal Telemetry v1
 * ESP-IDF Version
 * 
 * Main application entry point
 */

#include <string>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "OTAManager.h"
#include "SignalTelemetry.h"
#include "cJSON.h"

static const char *TAG = "main";

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

void handleMQTTMessage(std::string topic, std::string payload, size_t payloadLen);
void publishDeviceInfo();
void publishOTAStatus(const std::string& status);

// Helper function to get uptime in milliseconds
unsigned long millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32 Vault - Signal Telemetry v1");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "");
    
    // Initialize WiFi Manager
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifiManager.begin();
    
    // Wait for WiFi connection (with timeout)
    int retry_count = 0;
    while (!wifiManager.isConnected() && retry_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry_count++;
    }
    
    // Initialize MQTT Manager if WiFi is connected
    if (wifiManager.isConnected()) {
        ESP_LOGI(TAG, "Initializing MQTT...");
        mqttManager.begin();
        mqttManager.setCallback(handleMQTTMessage);
        
        // Initialize OTA with callback for status publishing
        ESP_LOGI(TAG, "Initializing OTA...");
        std::string deviceId = mqttManager.getMacAddress();
        otaManager.begin(deviceId);
        otaManager.setStatusCallback(publishOTAStatus);
        
        // Initialize Signal Telemetry
        ESP_LOGI(TAG, "Initializing Signal Telemetry...");
        signalTelemetry.begin(&mqttManager, mqttManager.getMacAddress());
        signalTelemetry.onBoot();
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Setup Complete!");
    ESP_LOGI(TAG, "Firmware: Signal Telemetry v1");
    ESP_LOGI(TAG, "Philosophy: Firmware = Oscilloscope, Server = Judge");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "");
    
    // Main loop
    while (1) {
        // Handle managers
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
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void handleMQTTMessage(std::string topic, std::string payload, size_t payloadLen) {
    ESP_LOGI(TAG, "Processing message: %s = %s", topic.c_str(), payload.c_str());
    
    // Handle MQTT broker configuration
    if (topic.find("/cmd/mqtt") != std::string::npos) {
        cJSON *json = cJSON_Parse(payload.c_str());
        if (json) {
            cJSON *server = cJSON_GetObjectItem(json, "server");
            cJSON *port = cJSON_GetObjectItem(json, "port");
            cJSON *user = cJSON_GetObjectItem(json, "user");
            cJSON *password = cJSON_GetObjectItem(json, "password");
            
            if (server && cJSON_IsString(server)) {
                std::string serverStr = server->valuestring;
                int portNum = port && cJSON_IsNumber(port) ? port->valueint : 1883;
                std::string userStr = user && cJSON_IsString(user) ? user->valuestring : "";
                std::string passStr = password && cJSON_IsString(password) ? password->valuestring : "";
                
                mqttManager.saveConfig(serverStr, portNum, userStr, passStr);
                mqttManager.publishStatus("mqtt_config_updated");
                ESP_LOGI(TAG, "MQTT configuration updated via MQTT");
            }
            cJSON_Delete(json);
        }
    }
    // Handle OTA update command
    else if (topic.find("/cmd/ota_update") != std::string::npos) {
        otaManager.handleUpdateCommand(payload);
    }
    // Handle restart command
    else if (topic.find("/cmd/restart") != std::string::npos) {
        mqttManager.publishStatus("restarting");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    // Handle WiFi credential update command
    else if (topic.find("/cmd/wifi") != std::string::npos) {
        cJSON *json = cJSON_Parse(payload.c_str());
        if (json) {
            cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
            cJSON *password = cJSON_GetObjectItem(json, "password");
            
            if (ssid && cJSON_IsString(ssid) && password && cJSON_IsString(password)) {
                std::string ssidStr = ssid->valuestring;
                std::string passStr = password->valuestring;
                
                if (!ssidStr.empty() && !passStr.empty()) {
                    wifiManager.saveCredentials(ssidStr, passStr);
                    mqttManager.publishStatus("wifi_credentials_updated");
                    ESP_LOGI(TAG, "WiFi credentials updated via MQTT");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else {
                    mqttManager.publishStatus("wifi_update_failed_invalid_params");
                    ESP_LOGI(TAG, "Invalid WiFi credentials provided");
                }
            }
            cJSON_Delete(json);
        }
    }
    // Handle WiFi reset command
    else if (topic.find("/cmd/reset_wifi") != std::string::npos) {
        wifiManager.clearCredentials();
        mqttManager.publishStatus("wifi_reset");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    // Handle signal pin configuration
    else if (topic.find("/cmd/signal/config") != std::string::npos) {
        cJSON *json = cJSON_Parse(payload.c_str());
        if (json) {
            cJSON *pin = cJSON_GetObjectItem(json, "pin");
            cJSON *captureRaw = cJSON_GetObjectItem(json, "capture_raw");
            cJSON *capturePulse = cJSON_GetObjectItem(json, "capture_pulse");
            cJSON *useRMT = cJSON_GetObjectItem(json, "use_rmt");
            
            if (pin && cJSON_IsNumber(pin)) {
                uint8_t pinNum = pin->valueint;
                bool raw = captureRaw && cJSON_IsTrue(captureRaw);
                bool pulse = capturePulse && cJSON_IsTrue(capturePulse);
                bool rmt = useRMT && cJSON_IsTrue(useRMT);
                
                if (signalTelemetry.configurePin(pinNum, raw, pulse, rmt)) {
                    mqttManager.publishStatus("signal_pin_configured");
                    ESP_LOGI(TAG, "Signal pin %d configured", pinNum);
                } else {
                    mqttManager.publishStatus("signal_pin_config_failed");
                    ESP_LOGI(TAG, "Signal pin configuration failed");
                }
            }
            cJSON_Delete(json);
        }
    }
    // Handle signal pin removal
    else if (topic.find("/cmd/signal/remove") != std::string::npos) {
        cJSON *json = cJSON_Parse(payload.c_str());
        if (json) {
            cJSON *pin = cJSON_GetObjectItem(json, "pin");
            
            if (pin && cJSON_IsNumber(pin)) {
                uint8_t pinNum = pin->valueint;
                
                if (signalTelemetry.removePin(pinNum)) {
                    mqttManager.publishStatus("signal_pin_removed");
                    ESP_LOGI(TAG, "Signal pin %d removed", pinNum);
                } else {
                    mqttManager.publishStatus("signal_pin_remove_failed");
                    ESP_LOGI(TAG, "Signal pin removal failed");
                }
            }
            cJSON_Delete(json);
        }
    }
}

void publishDeviceInfo() {
    if (!mqttManager.isConnected()) {
        return;
    }
    
    cJSON *json = cJSON_CreateObject();
    
    // Device information
    cJSON_AddStringToObject(json, "device_id", mqttManager.getMacAddress().c_str());
    cJSON_AddNumberToObject(json, "uptime", millis() / 1000);
    cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(json, "wifi_rssi", wifiManager.getRSSI());
    cJSON_AddStringToObject(json, "wifi_ssid", wifiManager.getSSID().c_str());
    cJSON_AddStringToObject(json, "ip_address", wifiManager.getIPAddress().c_str());
    cJSON_AddBoolToObject(json, "mqtt_connected", mqttManager.isConnected());
    cJSON_AddBoolToObject(json, "ota_update_in_progress", otaManager.isUpdateInProgress());
    cJSON_AddStringToObject(json, "firmware_version", "Signal Telemetry v1");
    
    // Add diagnostics
    DiagPacket diag = signalTelemetry.getDiagnostics();
    cJSON_AddNumberToObject(json, "dropped_raw", diag.droppedRaw);
    cJSON_AddNumberToObject(json, "dropped_pulse", diag.droppedPulse);
    cJSON_AddNumberToObject(json, "queue_depth", diag.queueDepth);
    
    char *output = cJSON_Print(json);
    mqttManager.publishStatus(output);
    free(output);
    cJSON_Delete(json);
}

void publishOTAStatus(const std::string& status) {
    if (mqttManager.isConnected()) {
        std::string deviceId = mqttManager.getMacAddress();
        std::string topic = "esp32vault/" + deviceId + "/ota/status";
        mqttManager.publish(topic, status);
    }
}
