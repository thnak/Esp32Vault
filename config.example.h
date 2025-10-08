/**
 * Configuration Example for ESP32 Vault
 * 
 * This file demonstrates how to customize various settings.
 * Copy this file to include/config.h and modify as needed.
 * 
 * Note: These are optional - the system works without this file
 * using default values and runtime configuration via MQTT.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration (Optional - for development)
// ============================================
// Uncomment to set default WiFi credentials
// WARNING: Don't commit credentials to version control!
// #define DEFAULT_WIFI_SSID "YourWiFiNetwork"
// #define DEFAULT_WIFI_PASSWORD "YourWiFiPassword"

// WiFi AP Mode Configuration
#define AP_PASSWORD "12345678"  // Change this to a stronger password!
#define AP_TIMEOUT_MS 10000     // How long to wait for WiFi connection

// ============================================
// MQTT Configuration (Optional - for development)
// ============================================
// Uncomment to set default MQTT broker
// #define DEFAULT_MQTT_SERVER "mqtt.example.com"
// #define DEFAULT_MQTT_PORT 1883
// #define DEFAULT_MQTT_USER "username"
// #define DEFAULT_MQTT_PASSWORD "password"

// MQTT Settings
#define MQTT_RECONNECT_DELAY 5000  // Delay between reconnection attempts (ms)
#define MQTT_BUFFER_SIZE 512       // MQTT message buffer size

// ============================================
// OTA Configuration
// ============================================
// Uncomment to set OTA password (recommended for production!)
// #define OTA_PASSWORD "your-secure-password"

// OTA Hostname (default: ESP32-Vault-{MAC})
// #define OTA_HOSTNAME "ESP32-Vault-Custom"

// ============================================
// Device Configuration
// ============================================
// Status update interval (milliseconds)
#define STATUS_INTERVAL 30000  // 30 seconds

// Serial baud rate
#define SERIAL_BAUD_RATE 115200

// Device name prefix (used for AP SSID and MQTT topics)
#define DEVICE_PREFIX "ESP32-Vault"

// ============================================
// Debug Configuration
// ============================================
// Uncomment to enable debug output
// #define DEBUG_WIFI
// #define DEBUG_MQTT
// #define DEBUG_OTA

// ============================================
// Advanced Settings
// ============================================

// Web Server Port (AP mode)
#define WEB_SERVER_PORT 80

// Maximum number of reconnection attempts before restarting AP mode
#define MAX_WIFI_RECONNECT_ATTEMPTS 3

// NVS (Non-Volatile Storage) namespaces
#define NVS_WIFI_NAMESPACE "wifi"
#define NVS_MQTT_NAMESPACE "mqtt"

// ============================================
// Feature Flags
// ============================================
// Uncomment to disable features
// #define DISABLE_OTA
// #define DISABLE_MQTT
// #define DISABLE_WEB_SERVER

// ============================================
// Pin Definitions (for future sensor integration)
// ============================================
// Example: LED pin for status indication
// #define LED_PIN 2

// Example: Button pin for reset
// #define RESET_BUTTON_PIN 0

#endif // CONFIG_H
