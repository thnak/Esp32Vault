#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>

// Forward declaration for callback
typedef std::function<void(const String& status)> OTAStatusCallback;

class OTAManager {
private:
    String deviceId;
    bool updateInProgress;
    OTAStatusCallback statusCallback;
    
    bool verifyIntegrity(const String& integrity);
    void publishProgress(int progress);
    void publishStatus(const String& status);

public:
    OTAManager();
    
    void begin(const String& deviceId = "");
    void loop();
    bool isUpdateInProgress();
    void setStatusCallback(OTAStatusCallback callback);
    
    // Handle OTA update command from MQTT
    void handleUpdateCommand(const String& payload);
};

#endif // OTA_MANAGER_H
