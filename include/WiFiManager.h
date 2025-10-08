#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

class WiFiManager {
private:
    Preferences preferences;
    WebServer* server;
    String ssid;
    String password;
    bool apMode;
    unsigned long connectTimeout;
    
    void startAP();
    void setupWebServer();
    void handleRoot();
    void handleSave();
    void handleStatus();

public:
    WiFiManager();
    ~WiFiManager();
    
    void begin();
    void loop();
    bool isConnected();
    bool isAPMode();
    void startConfigPortal();
    bool loadCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void clearCredentials();
};

#endif // WIFI_MANAGER_H
