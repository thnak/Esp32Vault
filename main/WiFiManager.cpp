#include "WiFiManager.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <cstring>

static const char *TAG = "WiFiManager";

// Default hotspot credentials
#define DEFAULT_SSID "EspSetup"
#define DEFAULT_PASSWORD "HeLooWod"

WiFiManager::WiFiManager() : nvsHandle(0), connected(false), retryCount(0) {
}

WiFiManager::~WiFiManager() {
    if (nvsHandle) {
        nvs_close(nvsHandle);
    }
}

void WiFiManager::begin() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Open NVS
    ret = nvs_open("wifi", NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
    }
    
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        this,
                                                        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        this,
                                                        nullptr));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Load credentials and connect
    if (loadCredentials()) {
        ESP_LOGI(TAG, "Loaded credentials for SSID: %s", ssid.c_str());
    } else {
        ESP_LOGI(TAG, "No saved credentials, using default hotspot");
        ssid = DEFAULT_SSID;
        password = DEFAULT_PASSWORD;
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialized, connecting to %s...", ssid.c_str());
}

void WiFiManager::loop() {
    // ESP-IDF handles everything in background tasks
    // Keep for API compatibility
}

bool WiFiManager::isConnected() {
    return connected;
}

bool WiFiManager::loadCredentials() {
    if (!nvsHandle) return false;
    
    // Load SSID
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(nvsHandle, "ssid", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buffer = new char[required_size];
        nvs_get_str(nvsHandle, "ssid", buffer, &required_size);
        ssid = buffer;
        delete[] buffer;
    } else {
        return false;
    }
    
    // Load password
    err = nvs_get_str(nvsHandle, "password", nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buffer = new char[required_size];
        nvs_get_str(nvsHandle, "password", buffer, &required_size);
        password = buffer;
        delete[] buffer;
    } else {
        return false;
    }
    
    return true;
}

void WiFiManager::saveCredentials(const std::string& new_ssid, const std::string& new_password) {
    if (!nvsHandle) return;
    
    ssid = new_ssid;
    password = new_password;
    
    nvs_set_str(nvsHandle, "ssid", ssid.c_str());
    nvs_set_str(nvsHandle, "password", password.c_str());
    nvs_commit(nvsHandle);
    
    ESP_LOGI(TAG, "WiFi credentials saved for SSID: %s", ssid.c_str());
}

void WiFiManager::clearCredentials() {
    if (!nvsHandle) return;
    
    nvs_erase_key(nvsHandle, "ssid");
    nvs_erase_key(nvsHandle, "password");
    nvs_commit(nvsHandle);
    
    ESP_LOGI(TAG, "WiFi credentials cleared");
}

int WiFiManager::getRSSI() {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

std::string WiFiManager::getIPAddress() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
            return std::string(ip_str);
        }
    }
    return "0.0.0.0";
}

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    manager->handleWiFiEvent(event_base, event_id, event_data);
}

void WiFiManager::ip_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    manager->handleWiFiEvent(event_base, event_id, event_data);
}

void WiFiManager::handleWiFiEvent(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        retryCount = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        retryCount++;
        
        // Check if we should retry (infinite if MAX_RETRY_COUNT == -1)
        if (MAX_RETRY_COUNT == -1 || retryCount <= MAX_RETRY_COUNT) {
            ESP_LOGI(TAG, "Disconnected from WiFi, reconnecting... (attempt %d)", retryCount);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Max retry count reached, stopping reconnection attempts");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        connected = true;
        retryCount = 0; // Reset retry count on successful connection
    }
}
