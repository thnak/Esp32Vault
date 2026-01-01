#ifndef WIFI_MANAGER_ESP_IDF_H
#define WIFI_MANAGER_ESP_IDF_H

#include <string>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"

class WiFiManager {
private:
    nvs_handle_t nvsHandle;
    bool connected;
    
    std::string ssid;
    std::string password;
    
    int retryCount;
    // MAX_RETRY_COUNT: -1 for infinite retries, or set to positive number for limited retries
    static const int MAX_RETRY_COUNT = -1;
    
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    
    void handleWiFiEvent(esp_event_base_t event_base, int32_t event_id, void* event_data);
    
public:
    WiFiManager();
    ~WiFiManager();
    
    void begin();
    void loop();
    bool isConnected();
    
    bool loadCredentials();
    void saveCredentials(const std::string& ssid, const std::string& password);
    void clearCredentials();
    
    std::string getSSID() const { return ssid; }
    int getRSSI();
    std::string getIPAddress();
};

#endif // WIFI_MANAGER_ESP_IDF_H
