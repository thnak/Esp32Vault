#include "WiFiManager.h"

WiFiManager::WiFiManager() : connectTimeout(10000) {
}

WiFiManager::~WiFiManager() {
}

void WiFiManager::begin() {
    preferences.begin("wifi", false);
    
    if (loadCredentials()) {
        Serial.println("Attempting to connect to saved WiFi...");
        if (connectToWiFi(ssid, password)) {
            Serial.println("WiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Failed to connect to saved WiFi.");
            Serial.println("Attempting to connect to default hotspot for provisioning...");
            
            if (connectToWiFi(DEFAULT_SSID, DEFAULT_PASSWORD)) {
                Serial.println("Connected to default hotspot!");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                Serial.println("Use MQTT to configure new WiFi credentials.");
            } else {
                Serial.println("Failed to connect to default hotspot.");
                Serial.println("Waiting for network connection...");
            }
        }
    } else {
        Serial.println("No saved credentials. Attempting to connect to default hotspot...");
        if (connectToWiFi(DEFAULT_SSID, DEFAULT_PASSWORD)) {
            Serial.println("Connected to default hotspot!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.println("Use MQTT to configure new WiFi credentials.");
        } else {
            Serial.println("Failed to connect to default hotspot.");
            Serial.println("Waiting for network connection...");
        }
    }
}

void WiFiManager::loop() {
    // No server to handle anymore
    delay(1);
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isAPMode() {
    return false; // No longer using AP mode
}

bool WiFiManager::connectToWiFi(const String& ssid, const String& password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < connectTimeout) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::loadCredentials() {
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    
    return (ssid.length() > 0 && password.length() > 0);
}

void WiFiManager::saveCredentials(const String& newSSID, const String& newPassword) {
    ssid = newSSID;
    password = newPassword;
    
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    
    Serial.println("WiFi credentials saved");
}

void WiFiManager::clearCredentials() {
    preferences.clear();
    Serial.println("WiFi credentials cleared");
}
