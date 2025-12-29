#include "MQTTManager.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <cstring>

static const char *TAG = "MQTTManager";

MQTTManager::MQTTManager() : mqttClient(nullptr), nvsHandle(0), mqttPort(1883), connected(false) {
    // Use MAC address as client ID per MQTT5 spec
    clientId = getMacAddress();
    baseTopic = "esp32vault/" + clientId;
}

MQTTManager::~MQTTManager() {
    if (mqttClient) {
        esp_mqtt_client_stop(mqttClient);
        esp_mqtt_client_destroy(mqttClient);
    }
    
    if (nvsHandle) {
        nvs_close(nvsHandle);
    }
}

void MQTTManager::begin() {
    // Open NVS
    esp_err_t err = nvs_open("mqtt", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }
    
    if (loadConfig()) {
        ESP_LOGI(TAG, "MQTT configuration loaded");
        
        // Configure MQTT5 client
        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = mqttServer.c_str();
        mqtt_cfg.broker.address.port = mqttPort;
        mqtt_cfg.credentials.client_id = clientId.c_str();
        
        // MQTT5 specific settings
        mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_5;
        mqtt_cfg.session.keepalive = 60;
        mqtt_cfg.session.disable_clean_session = false; // Clean start
        
        if (!mqttUser.empty()) {
            mqtt_cfg.credentials.username = mqttUser.c_str();
            mqtt_cfg.credentials.authentication.password = mqttPassword.c_str();
        }
        
        // Buffer size for binary payloads
        mqtt_cfg.buffer.size = 2048;
        mqtt_cfg.buffer.out_size = 2048;
        
        mqttClient = esp_mqtt_client_init(&mqtt_cfg);
        
        if (mqttClient) {
            esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, 
                                          mqtt_event_handler, this);
            esp_mqtt_client_start(mqttClient);
        } else {
            ESP_LOGE(TAG, "Failed to initialize MQTT client");
        }
    } else {
        ESP_LOGI(TAG, "No MQTT configuration found");
        // connect to default broker
        mqttServer = "45.251.112.69";
        mqttPort = 2704;
        saveConfig(mqttServer, mqttPort, "", "");
        begin();
    }
}

void MQTTManager::loop() {
    // ESP-IDF MQTT client handles everything in background tasks
    // This method is kept for API compatibility
}

bool MQTTManager::isConnected() {
    return connected;
}

void MQTTManager::setCallback(MQTTCallback callback) {
    messageCallback = callback;
}

void MQTTManager::setServer(const std::string& server, int port) {
    mqttServer = server;
    mqttPort = port;
}

void MQTTManager::setCredentials(const std::string& user, const std::string& password) {
    mqttUser = user;
    mqttPassword = password;
}

bool MQTTManager::loadConfig() {
    if (!nvsHandle) return false;
    
    // Load server
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(nvsHandle, "server", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buffer = new char[required_size];
        nvs_get_str(nvsHandle, "server", buffer, &required_size);
        mqttServer = buffer;
        delete[] buffer;
    } else {
        return false;
    }
    
    // Load port
    int32_t port;
    err = nvs_get_i32(nvsHandle, "port", &port);
    if (err == ESP_OK) {
        mqttPort = port;
    } else {
        mqttPort = 1883;
    }
    
    // Load user (optional)
    err = nvs_get_str(nvsHandle, "user", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buffer = new char[required_size];
        nvs_get_str(nvsHandle, "user", buffer, &required_size);
        mqttUser = buffer;
        delete[] buffer;
    }
    
    // Load password (optional)
    err = nvs_get_str(nvsHandle, "password", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buffer = new char[required_size];
        nvs_get_str(nvsHandle, "password", buffer, &required_size);
        mqttPassword = buffer;
        delete[] buffer;
    }
    
    return true;
}

void MQTTManager::saveConfig(const std::string& server, int port, const std::string& user, const std::string& password) {
    if (!nvsHandle) return;
    
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    
    nvs_set_str(nvsHandle, "server", mqttServer.c_str());
    nvs_set_i32(nvsHandle, "port", mqttPort);
    nvs_set_str(nvsHandle, "user", mqttUser.c_str());
    nvs_set_str(nvsHandle, "password", mqttPassword.c_str());
    nvs_commit(nvsHandle);
    
    ESP_LOGI(TAG, "MQTT configuration saved");
}

void MQTTManager::publish(const std::string& topic, const std::string& payload, bool retained) {
    if (mqttClient && connected) {
        // For JSON/text payloads, use MQTT5 properties with UTF-8 format indicator
        esp_mqtt5_publish_property_config_t publish_property = {};
        publish_property.payload_format_indicator = 1; // UTF-8
        publish_property.content_type = CONTENT_TYPE_JSON;
        
        // Set MQTT5 publish properties
        esp_mqtt5_client_set_publish_property(mqttClient, &publish_property);
        
        // Enqueue with store=true to ensure delivery even if offline
        int msg_id = esp_mqtt_client_enqueue(mqttClient, topic.c_str(), 
                                            payload.c_str(), payload.length(), 
                                            0, retained ? 1 : 0, true);
        
        ESP_LOGD(TAG, "Published JSON to %s, msg_id=%d", topic.c_str(), msg_id);
    }
}

void MQTTManager::publish(const std::string& topic, const uint8_t* payload, size_t length, bool retained) {
    // Default binary publish without content type (for backward compatibility)
    publishBinary(topic, payload, length, CONTENT_TYPE_RAW_SIGNAL, retained, 0);
}

void MQTTManager::publishBinary(const std::string& topic, const uint8_t* payload, size_t length, 
                                 const char* contentType, bool retained, uint32_t messageExpiryInterval) {
    if (mqttClient && connected) {
        // MQTT5 properties for binary payloads
        esp_mqtt5_publish_property_config_t publish_property = {};
        publish_property.payload_format_indicator = 0; // Binary
        publish_property.content_type = contentType;
        
        // Set message expiry interval if provided
        // messageExpiryInterval=0 means no expiry (message persists indefinitely)
        if (messageExpiryInterval > 0) {
            publish_property.message_expiry_interval = messageExpiryInterval;
        }
        
        // Set MQTT5 publish properties
        esp_mqtt5_client_set_publish_property(mqttClient, &publish_property);
        
        // Enqueue with store=true to ensure delivery even if offline
        int msg_id = esp_mqtt_client_enqueue(mqttClient, topic.c_str(), 
                                            (const char*)payload, length, 
                                            0, retained ? 1 : 0, true);
        
        ESP_LOGD(TAG, "Published binary to %s, msg_id=%d, len=%zu, content-type=%s", 
                 topic.c_str(), msg_id, length, contentType);
    }
}

void MQTTManager::subscribe(const std::string& topic) {
    if (mqttClient && connected) {
        int msg_id = esp_mqtt_client_subscribe(mqttClient, topic.c_str(), 0);
        ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", topic.c_str(), msg_id);
    }
}

void MQTTManager::publishStatus(const std::string& status) {
    std::string topic = baseTopic + "/status";
    publish(topic, status, true);
}

void MQTTManager::publishConfig(const std::string& config) {
    std::string topic = baseTopic + "/config";
    publish(topic, config, true);
}

std::string MQTTManager::getMacAddress() const {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(macStr);
}

void MQTTManager::mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                     int32_t event_id, void *event_data) {
    MQTTManager* manager = static_cast<MQTTManager*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    manager->handleMQTTEvent(event);
}

void MQTTManager::handleMQTTEvent(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            connected = true;
            
            // Subscribe to command topics
            subscribe(baseTopic + "/cmd/#");
            
            // Publish online status
            publishStatus("online");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            connected = false;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            if (messageCallback) {
                std::string topic(event->topic, event->topic_len);
                std::string payload(event->data, event->data_len);
                messageCallback(topic, payload, event->data_len);
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                        strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}
