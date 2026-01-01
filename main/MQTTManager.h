#ifndef MQTT_MANAGER_ESP_IDF_H
#define MQTT_MANAGER_ESP_IDF_H

#include <string>
#include <functional>
#include "esp_log.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "nvs.h"

typedef std::function<void(std::string topic, std::string payload, size_t payloadLen)> MQTTCallback;

// MQTT5 Content Types for Signal Telemetry
#define CONTENT_TYPE_RAW_SIGNAL    "application/vnd.esp32vault.signal.raw+bin"
#define CONTENT_TYPE_PULSE_SIGNAL  "application/vnd.esp32vault.signal.pulse+bin"
#define CONTENT_TYPE_DIAG_SIGNAL   "application/vnd.esp32vault.signal.diag+bin"
#define CONTENT_TYPE_JSON          "application/json"

// MQTT5 Message Expiry for Telemetry (seconds)
#define MESSAGE_EXPIRY_TELEMETRY_SECONDS  60

class MQTTManager {
private:
    esp_mqtt_client_handle_t mqttClient;
    nvs_handle_t nvsHandle;
    
    std::string mqttServer;
    int mqttPort;
    std::string mqttUser;
    std::string mqttPassword;
    std::string clientId;
    std::string baseTopic;
    
    MQTTCallback messageCallback;
    bool connected;
    
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                   int32_t event_id, void *event_data);
    void handleMQTTEvent(esp_mqtt_event_handle_t event);
    
public:
    MQTTManager();
    ~MQTTManager();
    
    void begin();
    void loop();
    bool isConnected();
    void reconnect(); // Manually trigger reconnection
    
    void setCallback(MQTTCallback callback);
    void setServer(const std::string& server, int port);
    void setCredentials(const std::string& user, const std::string& password);
    
    bool loadConfig();
    void saveConfig(const std::string& server, int port, const std::string& user, const std::string& password);
    
    void publish(const std::string& topic, const std::string& payload, bool retained = false);
    void publish(const std::string& topic, const uint8_t* payload, size_t length, bool retained = false);
    void publishBinary(const std::string& topic, const uint8_t* payload, size_t length, 
                       const char* contentType, bool retained = false, uint32_t messageExpiryInterval = 0);
    void subscribe(const std::string& topic);
    
    void publishStatus(const std::string& status);
    void publishConfig(const std::string& config);
    
    std::string getClientId() const { return clientId; }
    std::string getMacAddress() const;
};

#endif // MQTT_MANAGER_ESP_IDF_H
