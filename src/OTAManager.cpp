#include "OTAManager.h"

OTAManager::OTAManager() : enabled(false) {
}

void OTAManager::begin(const String& host) {
    if (host.length() > 0) {
        hostname = host;
    } else {
        hostname = "ESP32-Vault-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    }
    
    ArduinoOTA.setHostname(hostname.c_str());
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    
    ArduinoOTA.begin();
    enabled = true;
    
    Serial.println("OTA Ready");
    Serial.print("Hostname: ");
    Serial.println(hostname);
}

void OTAManager::loop() {
    if (enabled) {
        ArduinoOTA.handle();
    }
}

void OTAManager::setPassword(const String& password) {
    ArduinoOTA.setPassword(password.c_str());
}

bool OTAManager::isEnabled() {
    return enabled;
}
