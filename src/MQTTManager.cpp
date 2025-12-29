#include "MQTTManager.h"
#include <esp_system.h>

MQTTManager::MQTTManager() : mqttPort(1883), lastReconnectAttempt(0) {
    mqttClient = new PubSubClient(wifiClient);
    // Use MAC address as client ID per spec
    clientId = getMacAddress();
    baseTopic = "esp32vault/" + clientId;
    // Increase buffer size for binary packets
    mqttClient->setBufferSize(2048);
}

MQTTManager::~MQTTManager() {
    if (mqttClient != nullptr) {
        delete mqttClient;
    }
}

void MQTTManager::begin() {
    preferences.begin("mqtt", false);
    
    if (loadConfig()) {
        Serial.println("MQTT configuration loaded");
        mqttClient->setServer(mqttServer.c_str(), mqttPort);
        mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->callback(topic, payload, length);
        });
    } else {
        Serial.println("No MQTT configuration found");
    }
}

void MQTTManager::loop() {
    if (mqttClient->connected()) {
        mqttClient->loop();
    } else {
        if (WiFi.status() == WL_CONNECTED) {
            unsigned long now = millis();
            if (now - lastReconnectAttempt > 5000) {
                lastReconnectAttempt = now;
                if (reconnect()) {
                    lastReconnectAttempt = 0;
                }
            }
        }
    }
}

bool MQTTManager::isConnected() {
    return mqttClient->connected();
}

void MQTTManager::setCallback(MQTTCallback callback) {
    messageCallback = callback;
}

void MQTTManager::setServer(const String& server, int port) {
    mqttServer = server;
    mqttPort = port;
    mqttClient->setServer(mqttServer.c_str(), mqttPort);
}

void MQTTManager::setCredentials(const String& user, const String& password) {
    mqttUser = user;
    mqttPassword = password;
}

bool MQTTManager::loadConfig() {
    mqttServer = preferences.getString("server", "");
    mqttPort = preferences.getInt("port", 1883);
    mqttUser = preferences.getString("user", "");
    mqttPassword = preferences.getString("password", "");
    
    return (mqttServer.length() > 0);
}

void MQTTManager::saveConfig(const String& server, int port, const String& user, const String& password) {
    mqttServer = server;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    
    preferences.putString("server", mqttServer);
    preferences.putInt("port", mqttPort);
    preferences.putString("user", mqttUser);
    preferences.putString("password", mqttPassword);
    
    Serial.println("MQTT configuration saved");
    
    mqttClient->setServer(mqttServer.c_str(), mqttPort);
}

void MQTTManager::publish(const String& topic, const String& payload, bool retained) {
    if (mqttClient->connected()) {
        mqttClient->publish(topic.c_str(), payload.c_str(), retained);
    }
}

void MQTTManager::publish(const String& topic, const uint8_t* payload, size_t length, bool retained) {
    if (mqttClient->connected()) {
        mqttClient->publish(topic.c_str(), payload, length, retained);
    }
}

String MQTTManager::getMacAddress() const {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}


void MQTTManager::subscribe(const String& topic) {
    if (mqttClient->connected()) {
        mqttClient->subscribe(topic.c_str());
        Serial.print("Subscribed to: ");
        Serial.println(topic);
    }
}

void MQTTManager::publishStatus(const String& status) {
    String topic = baseTopic + "/status";
    publish(topic, status, true);
}

void MQTTManager::publishConfig(const String& config) {
    String topic = baseTopic + "/config";
    publish(topic, config, true);
}

void MQTTManager::publishSignalStrength(int rssi) {
    String topic = baseTopic + "/signal/strenght";
    publish(topic, String(rssi), false);
}

bool MQTTManager::reconnect() {
    Serial.print("Attempting MQTT connection...");
    
    bool connected = false;
    if (mqttUser.length() > 0) {
        connected = mqttClient->connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str());
    } else {
        connected = mqttClient->connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("connected");
        
        // Subscribe to command topics
        subscribe(baseTopic + "/cmd/#");
        subscribe(baseTopic + "/config/set");
        
        // Publish online status
        publishStatus("online");
        
        return true;
    } else {
        Serial.print("failed, rc=");
        Serial.print(mqttClient->state());
        Serial.println(" will try again in 5 seconds");
        return false;
    }
}

void MQTTManager::callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr = "";
    
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    Serial.print("Message arrived [");
    Serial.print(topicStr);
    Serial.print("]: ");
    Serial.println(payloadStr);
    
    if (messageCallback) {
        messageCallback(topicStr, payloadStr);
    }
}
