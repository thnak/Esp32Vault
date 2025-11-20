#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <Preferences.h>

class WiFiManager {
private:
    Preferences preferences;
    String ssid;
    String password;
    unsigned long connectTimeout;
    
    // Predefined hotspot credentials for initial setup
    const char* DEFAULT_SSID = "EspSetup";
    const char* DEFAULT_PASSWORD = "HeLooWod";
    
    bool connectToWiFi(const String& ssid, const String& password);

public:
    WiFiManager();
    ~WiFiManager();
    
    void begin();
    void loop();
    bool isConnected();
    bool isAPMode();
    bool loadCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void clearCredentials();
};

#endif // WIFI_MANAGER_H
