#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

typedef std::function<void(String topic, String payload)> MQTTCallback;

class MQTTManager {
private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    Preferences preferences;
    
    String mqttServer;
    int mqttPort;
    String mqttUser;
    String mqttPassword;
    String clientId;
    String baseTopic;
    
    MQTTCallback messageCallback;
    unsigned long lastReconnectAttempt;
    
    void callback(char* topic, byte* payload, unsigned int length);
    bool reconnect();

public:
    MQTTManager();
    ~MQTTManager();
    
    void begin();
    void loop();
    bool isConnected();
    
    void setCallback(MQTTCallback callback);
    void setServer(const String& server, int port);
    void setCredentials(const String& user, const String& password);
    
    bool loadConfig();
    void saveConfig(const String& server, int port, const String& user, const String& password);
    
    void publish(const String& topic, const String& payload, bool retained = false);
    void subscribe(const String& topic);
    
    void publishStatus(const String& status);
    void publishConfig(const String& config);
    void publishSignalStrength(int rssi);
};

#endif // MQTT_MANAGER_H
