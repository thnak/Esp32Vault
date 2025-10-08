#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ArduinoOTA.h>

class OTAManager {
private:
    String hostname;
    bool enabled;

public:
    OTAManager();
    
    void begin(const String& hostname = "");
    void loop();
    void setPassword(const String& password);
    bool isEnabled();
};

#endif // OTA_MANAGER_H
