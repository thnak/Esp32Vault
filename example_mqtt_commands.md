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

### 2. Enable OTA Updates

Enable OTA functionality:

```bash
mosquitto_pub -h your-broker.com -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota" -m "enable"
```

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
  "ota_enabled": true
}
```

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

# Enable OTA
client.publish(f"{BASE_TOPIC}/cmd/ota", "enable")

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
    
    // Subscribe to status
    client.subscribe(`${BASE_TOPIC}/status`);
});

client.on('message', (topic, message) => {
    console.log(`${topic}: ${message.toString()}`);
});
```

## Topic Structure Reference

| Topic Pattern | Direction | Description |
|--------------|-----------|-------------|
| `esp32vault/{device_id}/status` | Device → Broker | Device status and telemetry |
| `esp32vault/{device_id}/config` | Device → Broker | Configuration data |
| `esp32vault/{device_id}/cmd/mqtt` | Broker → Device | Configure MQTT settings |
| `esp32vault/{device_id}/cmd/ota` | Broker → Device | Enable/disable OTA |
| `esp32vault/{device_id}/cmd/restart` | Broker → Device | Restart device |
| `esp32vault/{device_id}/cmd/reset_wifi` | Broker → Device | Reset WiFi credentials |
| `esp32vault/{device_id}/config/set` | Broker → Device | Update device configuration |

## Security Best Practices

1. **Use TLS/SSL**: Enable secure MQTT connections in production
2. **Strong Passwords**: Use strong passwords for MQTT authentication
3. **Topic ACLs**: Configure MQTT broker to restrict topic access
4. **Update Regularly**: Keep firmware updated via OTA
5. **Network Isolation**: Use VLANs or separate networks for IoT devices
