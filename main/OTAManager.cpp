#include "OTAManager.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "OTAManager";

OTAManager::OTAManager() : updateInProgress(false) {
}

OTAManager::~OTAManager() {
}

void OTAManager::begin(const std::string& devId) {
    deviceId = devId;
    ESP_LOGI(TAG, "OTA Manager initialized for device: %s", deviceId.c_str());
}

void OTAManager::loop() {
    // OTA handled via MQTT commands
}

void OTAManager::setStatusCallback(OTAStatusCallback callback) {
    statusCallback = callback;
}

void OTAManager::handleUpdateCommand(const std::string& payload) {
    cJSON *json = cJSON_Parse(payload.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse OTA command JSON");
        return;
    }
    
    cJSON *url_item = cJSON_GetObjectItem(json, "url");
    if (url_item == nullptr || !cJSON_IsString(url_item)) {
        ESP_LOGE(TAG, "No URL in OTA command");
        cJSON_Delete(json);
        return;
    }
    
    std::string url = url_item->valuestring;
    ESP_LOGI(TAG, "Starting OTA update from: %s", url.c_str());
    
    if (statusCallback) {
        statusCallback("{\"status\":\"starting\"}");
    }
    
    updateInProgress = true;
    
    esp_http_client_config_t http_config = {};
    http_config.url = url.c_str();
    http_config.timeout_ms = 30000;
    http_config.keep_alive_enable = true;
    
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;
    
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful, rebooting...");
        if (statusCallback) {
            statusCallback("{\"status\":\"success\"}");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        if (statusCallback) {
            statusCallback("{\"status\":\"error\"}");
        }
        updateInProgress = false;
    }
    
    cJSON_Delete(json);
}
