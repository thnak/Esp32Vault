# MQTT Command Examples

This document provides examples of MQTT commands you can use to control and configure your ESP32 Vault device.

## Prerequisites

- MQTT broker running (e.g., Mosquitto, HiveMQ)
- MQTT client (e.g., mosquitto_pub, MQTT Explorer, or any MQTT library)
- Device ID of your ESP32 (shown in serial output or AP SSID)

## Command Examples

### 1. Configure MQTT Broker

Set the MQTT broker address and credentials:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" -m '{
  "server": "mqtt.example.com",
  "port": 1883,
  "user": "username",
  "password": "password"
}'
```

### 2. Trigger OTA Update

Trigger a firmware update via HTTP(S):

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota_update" -m '{
  "version": "1.0.2",
  "url": "http://example.com/firmware/esp32vault-v1.0.2.bin",
  "integrity": "sha256:abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"
}'
```

**Parameters:**
- `version` (required): Version identifier for logging and tracking
- `url` (required): HTTP(S) URL to download the firmware binary
- `integrity` (optional): SHA256 hash for future verification (currently noted for enhancement)

**Note:** The device will download the firmware, verify the binary format, flash it, and reboot automatically.

### 3. Restart Device

Restart the ESP32:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/restart" -m "1"
```

### 4. Reset WiFi Credentials

Clear saved WiFi credentials (device will restart in AP mode):

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/reset_wifi" -m "1"
```

### 5. Update Configuration

Update device configuration parameters:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/config/set" -m '{
  "status_interval": 60000
}'
```

## Subscribing to Device Status

### Subscribe to All Device Topics

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/#" -v
```

### Subscribe to Status Only

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/status" -v
```

### Subscribe to OTA Status

Monitor OTA update progress in real-time:

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/ota/status" -v
```

## Expected Status Messages

The device publishes status messages periodically (default: 30 seconds):

```json
{
  "device_id": "XXXXXXXX",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "wifi_ssid": "YourNetwork",
  "ip_address": "192.168.1.100",
  "mqtt_connected": true,
  "ota_update_in_progress": false
}
```

### OTA Status Messages

During an OTA update, the device publishes progress to `esp32vault/{device_id}/ota/status`:

**Starting:**
```json
{
  "status": "starting",
  "version": "1.0.2"
}
```

**Downloading:**
```json
{
  "status": "downloading"
}
```

**Progress Updates (every 10%):**
```json
{
  "status": "updating",
  "progress": 50
}
```

**Success:**
```json
{
  "status": "success",
  "message": "Update completed, rebooting..."
}
```

**Error:**
```json
{
  "status": "error",
  "error_code": -104,
  "message": "HTTP error description"
}
```

The device also publishes WiFi signal strength periodically (default: 10 seconds):

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/signal/strenght" -v
```

Signal strength is published as RSSI value (e.g., `-45`, `-67`). Lower negative values indicate stronger signal.

## Using Python with Paho MQTT

Example Python script to send commands:

```python
import paho.mqtt.client as mqtt
import json

# Configuration
BROKER = "your-broker.com"
DEVICE_ID = "ESP32-Vault-XXXXXXXX"
BASE_TOPIC = f"esp32vault/{DEVICE_ID}"

# Create client
client = mqtt.Client()
client.connect(BROKER, 1883, 60)

# Configure MQTT broker
mqtt_config = {
    "server": "mqtt.example.com",
    "port": 1883,
    "user": "username",
    "password": "password"
}
client.publish(f"{BASE_TOPIC}/cmd/mqtt", json.dumps(mqtt_config))

# Trigger OTA update
ota_update = {
    "version": "1.0.2",
    "url": "http://example.com/firmware.bin",
    "integrity": "sha256:abcdef..."
}
client.publish(f"{BASE_TOPIC}/cmd/ota_update", json.dumps(ota_update))

# Subscribe to status
def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client.on_message = on_message
client.subscribe(f"{BASE_TOPIC}/status")
client.loop_forever()
```

## Using Node.js with MQTT.js

Example Node.js script:

```javascript
const mqtt = require('mqtt');

const BROKER = 'mqtt://your-broker.com';
const DEVICE_ID = 'ESP32-Vault-XXXXXXXX';
const BASE_TOPIC = `esp32vault/${DEVICE_ID}`;

const client = mqtt.connect(BROKER);

client.on('connect', () => {
    console.log('Connected to MQTT broker');
    
    // Configure MQTT broker
    const mqttConfig = {
        server: 'mqtt.example.com',
        port: 1883,
        user: 'username',
        password: 'password'
    };
    
    client.publish(`${BASE_TOPIC}/cmd/mqtt`, JSON.stringify(mqttConfig));
    
    // Trigger OTA update
    const otaUpdate = {
        version: '1.0.2',
        url: 'http://example.com/firmware.bin',
        integrity: 'sha256:abcdef...'
    };
    client.publish(`${BASE_TOPIC}/cmd/ota_update`, JSON.stringify(otaUpdate));
    
    // Subscribe to status
    client.subscribe(`${BASE_TOPIC}/status`);
});

client.on('message', (topic, message) => {
    console.log(`${topic}: ${message.toString()}`);
});
```

## IO Management Commands

### 6. Configure Output Pin

Configure a pin as output:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" -m '{
  "pin": 13,
  "mode": "output",
  "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/13/state",
  "persist": true,
  "retain": false
}'
```

### 7. Configure Input Pin with Interrupt

Configure a pin as interrupt input:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" -m '{
  "pin": 14,
  "mode": "interrupt",
  "edge": "change",
  "debounce": 50,
  "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/14/state",
  "persist": true,
  "retain": false
}'
```

### 8. Configure Analog Input

Configure a pin for analog reading with periodic reporting:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" -m '{
  "pin": 36,
  "mode": "analog",
  "interval": 5000,
  "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/36/state",
  "persist": true,
  "retain": false
}'
```

### 9. Trigger Output Pin

Set pin HIGH:
```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" -m "set"
```

Set pin LOW:
```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" -m "reset"
```

Toggle pin:
```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" -m "toggle"
```

Pulse pin (100ms default):
```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" -m "pulse"
```

Pulse pin with custom duration:
```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" -m '{
  "action": "pulse",
  "pulse": 500
}'
```

### 10. Set Pin Exclusion List

Exclude specific pins from configuration:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/exclude" -m '{
  "pins": [0, 1, 3],
  "ranges": [{"from": 6, "to": 11}],
  "persist": true
}'
```

### 11. Subscribe to Pin State

Subscribe to a specific pin state:

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/io/14/state" -v
```

Subscribe to all pin states:

```bash
mosquitto_sub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/io/+/state" -v
```

## Topic Structure Reference

| Topic Pattern | Direction | Description |
|--------------|-----------|-------------|
| `esp32vault/{device_id}/status` | Device → Broker | Device status and telemetry |
| `esp32vault/{device_id}/signal/strenght` | Device → Broker | WiFi signal strength (RSSI) |
| `esp32vault/{device_id}/ota/status` | Device → Broker | OTA update progress and status |
| `esp32vault/{device_id}/config` | Device → Broker | Configuration data |
| `esp32vault/{device_id}/cmd/mqtt` | Broker → Device | Configure MQTT settings |
| `esp32vault/{device_id}/cmd/ota_update` | Broker → Device | Trigger OTA firmware update |
| `esp32vault/{device_id}/cmd/restart` | Broker → Device | Restart device |
| `esp32vault/{device_id}/cmd/reset_wifi` | Broker → Device | Reset WiFi credentials |
| `esp32vault/{device_id}/config/set` | Broker → Device | Update device configuration |
| `esp32vault/{device_id}/cmd/io/config` | Broker → Device | Configure IO pin |
| `esp32vault/{device_id}/cmd/io/exclude` | Broker → Device | Set pin exclusion list |
| `esp32vault/{device_id}/cmd/io/{pin}/trigger` | Broker → Device | Trigger output pin |
| `esp32vault/{device_id}/io/{pin}/state` | Device → Broker | Pin state report |

## Security Best Practices

1. **Use TLS/SSL**: Enable secure MQTT connections in production
2. **Strong Passwords**: Use strong passwords for MQTT authentication
3. **Topic ACLs**: Configure MQTT broker to restrict topic access
4. **Update Regularly**: Keep firmware updated via OTA
5. **Network Isolation**: Use VLANs or separate networks for IoT devices
