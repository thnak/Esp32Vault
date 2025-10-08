#include "WiFiManager.h"

WiFiManager::WiFiManager() : apMode(false), connectTimeout(10000) {
    server = nullptr;
}

WiFiManager::~WiFiManager() {
    if (server != nullptr) {
        delete server;
    }
}

void WiFiManager::begin() {
    preferences.begin("wifi", false);
    
    if (loadCredentials()) {
        Serial.println("Attempting to connect to saved WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < connectTimeout) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            apMode = false;
        } else {
            Serial.println("Failed to connect. Starting AP mode...");
            startConfigPortal();
        }
    } else {
        Serial.println("No saved credentials. Starting AP mode...");
        startConfigPortal();
    }
}

void WiFiManager::loop() {
    if (server != nullptr) {
        server->handleClient();
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isAPMode() {
    return apMode;
}

void WiFiManager::startConfigPortal() {
    apMode = true;
    startAP();
    setupWebServer();
}

void WiFiManager::startAP() {
    WiFi.mode(WIFI_AP);
    String apSSID = "ESP32-Vault-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.softAP(apSSID.c_str(), "12345678");
    
    Serial.println("AP Mode started");
    Serial.print("SSID: ");
    Serial.println(apSSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
}

void WiFiManager::setupWebServer() {
    if (server != nullptr) {
        delete server;
    }
    
    server = new WebServer(80);
    
    server->on("/", [this]() { this->handleRoot(); });
    server->on("/save", HTTP_POST, [this]() { this->handleSave(); });
    server->on("/status", [this]() { this->handleStatus(); });
    
    server->begin();
    Serial.println("Web server started on port 80");
}

void WiFiManager::handleRoot() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>ESP32 Vault WiFi Setup</title>
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 400px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        input { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; }
        button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background: #45a049; }
        .info { padding: 10px; background: #e7f3fe; border-left: 4px solid #2196F3; margin: 10px 0; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>ESP32 Vault</h1>
        <div class='info'>Configure WiFi credentials to connect</div>
        <form action='/save' method='POST'>
            <label>WiFi SSID:</label>
            <input type='text' name='ssid' required>
            <label>WiFi Password:</label>
            <input type='password' name='password' required>
            <button type='submit'>Save & Connect</button>
        </form>
    </div>
</body>
</html>
)";
    server->send(200, "text/html", html);
}

void WiFiManager::handleSave() {
    if (server->hasArg("ssid") && server->hasArg("password")) {
        String newSSID = server->arg("ssid");
        String newPassword = server->arg("password");
        
        saveCredentials(newSSID, newPassword);
        
        String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Saved</title>
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 400px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #4CAF50; text-align: center; }
        p { text-align: center; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>Configuration Saved!</h1>
        <p>ESP32 will restart and attempt to connect to WiFi.</p>
        <p>If connection fails, AP mode will restart.</p>
    </div>
</body>
</html>
)";
        server->send(200, "text/html", html);
        delay(2000);
        ESP.restart();
    } else {
        server->send(400, "text/plain", "Missing SSID or Password");
    }
}

void WiFiManager::handleStatus() {
    String status = "AP Mode";
    if (WiFi.status() == WL_CONNECTED) {
        status = "Connected to " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
    }
    server->send(200, "text/plain", status);
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
