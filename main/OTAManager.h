#ifndef OTA_MANAGER_ESP_IDF_H
#define OTA_MANAGER_ESP_IDF_H

#include <string>
#include <functional>
#include "esp_https_ota.h"

typedef std::function<void(const std::string& status)> OTAStatusCallback;

class OTAManager {
private:
    OTAStatusCallback statusCallback;
    bool updateInProgress;
    std::string deviceId;
    
public:
    OTAManager();
    ~OTAManager();
    
    void begin(const std::string& devId);
    void loop();
    
    void setStatusCallback(OTAStatusCallback callback);
    void handleUpdateCommand(const std::string& payload);
    
    bool isUpdateInProgress() const { return updateInProgress; }
};

#endif // OTA_MANAGER_ESP_IDF_H
