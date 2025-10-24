#include "OTAManager.h"

OTAManager::OTAManager() : updateInProgress(false) {
}

void OTAManager::begin(const String& id) {
    if (id.length() > 0) {
        deviceId = id;
    } else {
        deviceId = "ESP32-Vault-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    }
    
    Serial.println("HTTP OTA Manager Ready");
    Serial.print("Device ID: ");
    Serial.println(deviceId);
}

void OTAManager::loop() {
    // No polling needed for HTTP OTA - triggered by MQTT commands
}

bool OTAManager::isUpdateInProgress() {
    return updateInProgress;
}

void OTAManager::setStatusCallback(OTAStatusCallback callback) {
    statusCallback = callback;
}

void OTAManager::publishStatus(const String& status) {
    if (statusCallback) {
        statusCallback(status);
    }
    Serial.println("OTA Status: " + status);
}

void OTAManager::publishProgress(int progress) {
    StaticJsonDocument<128> doc;
    doc["progress"] = progress;
    doc["status"] = "updating";
    
    String output;
    serializeJson(doc, output);
    publishStatus(output);
}

bool OTAManager::verifyIntegrity(const String& integrity) {
    // Integrity check is handled by HTTPUpdate library's built-in verification
    // This method is kept for future custom integrity verification if needed
    return true;
}

void OTAManager::handleUpdateCommand(const String& payload) {
    if (updateInProgress) {
        publishStatus("{\"status\":\"error\",\"message\":\"Update already in progress\"}");
        return;
    }
    
    // Parse JSON payload
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        publishStatus("{\"status\":\"error\",\"message\":\"Invalid JSON payload\"}");
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Extract parameters
    String version = doc["version"] | "";
    String integrity = doc["integrity"] | "";
    String url = doc["url"] | "";
    
    if (version.isEmpty() || url.isEmpty()) {
        publishStatus("{\"status\":\"error\",\"message\":\"Missing required fields: version and url\"}");
        return;
    }
    
    Serial.println("\n=================================");
    Serial.println("Starting OTA Update");
    Serial.println("=================================");
    Serial.print("Version: ");
    Serial.println(version);
    Serial.print("URL: ");
    Serial.println(url);
    if (!integrity.isEmpty()) {
        Serial.print("Integrity: ");
        Serial.println(integrity);
    }
    
    updateInProgress = true;
    
    // Publish starting status
    StaticJsonDocument<256> statusDoc;
    statusDoc["status"] = "starting";
    statusDoc["version"] = version;
    String statusOutput;
    serializeJson(statusDoc, statusOutput);
    publishStatus(statusOutput);
    
    // Configure HTTPUpdate
    WiFiClient client;
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    // Set up progress callback
    httpUpdate.onProgress([this](int progress, int total) {
        int percentage = (progress * 100) / total;
        Serial.printf("Progress: %d%%\r", percentage);
        
        // Publish progress every 10%
        static int lastPercentage = -1;
        if (percentage - lastPercentage >= 10) {
            lastPercentage = percentage;
            this->publishProgress(percentage);
        }
    });
    
    // Set up callbacks for start and end
    httpUpdate.onStart([this]() {
        Serial.println("\nOTA Update Started");
        publishStatus("{\"status\":\"downloading\"}");
    });
    
    httpUpdate.onEnd([this]() {
        Serial.println("\nOTA Update Completed Successfully");
        publishStatus("{\"status\":\"success\",\"message\":\"Update completed, rebooting...\"}");
    });
    
    httpUpdate.onError([this](int error) {
        Serial.printf("\nHTTP Update Error: %d\n", error);
        String errorMsg = httpUpdate.getLastErrorString();
        
        StaticJsonDocument<256> errorDoc;
        errorDoc["status"] = "error";
        errorDoc["error_code"] = error;
        errorDoc["message"] = errorMsg;
        
        String errorOutput;
        serializeJson(errorDoc, errorOutput);
        publishStatus(errorOutput);
        
        updateInProgress = false;
    });
    
    // Note: Custom SHA256 integrity verification
    // The ESP32 HTTPUpdate library doesn't support SHA256 out of the box.
    // The Update library verifies the binary format and flash compatibility automatically.
    // For production use, consider implementing custom SHA256 verification or using
    // HTTPS with certificate validation to ensure firmware integrity.
    if (!integrity.isEmpty()) {
        Serial.print("Integrity check requested: ");
        Serial.println(integrity);
        Serial.println("Note: SHA256 verification not implemented in this version.");
        Serial.println("Using built-in Update library binary verification.");
        // Future enhancement: Download to buffer, verify SHA256, then flash
    }
    
    // Perform the update
    Serial.println("Starting firmware download and update...");
    t_httpUpdate_return ret = httpUpdate.update(client, url);
    
    // Handle result (onError/onEnd callbacks handle most cases)
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            // Already handled by onError callback
            break;
        
        case HTTP_UPDATE_NO_UPDATES:
            publishStatus("{\"status\":\"info\",\"message\":\"No updates available\"}");
            Serial.println("No updates available");
            updateInProgress = false;
            break;
        
        case HTTP_UPDATE_OK:
            // Already handled by onEnd callback
            // Device will reboot automatically
            Serial.println("Update successful, device will reboot...");
            delay(1000);
            ESP.restart();
            break;
    }
}
