# IO Management Usage Examples

This document provides practical examples of using the ESP32 Vault IO management features.

## Basic Setup

Before using IO management, ensure your device is:
1. Connected to WiFi
2. Connected to an MQTT broker
3. Device ID noted (shown in serial output or AP SSID)

## Example 1: Simple LED Control

Control an LED on GPIO13:

### Step 1: Configure the pin as output
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" \
  -m '{
    "pin": 13,
    "mode": "output",
    "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/13/state",
    "persist": true
  }'
```

### Step 2: Turn LED on
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" \
  -m "set"
```

### Step 3: Turn LED off
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" \
  -m "reset"
```

### Step 4: Make LED blink
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger" \
  -m "toggle"
```

## Example 2: Button Input with Interrupt

Monitor a button on GPIO14 with interrupt:

### Configure the pin
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" \
  -m '{
    "pin": 14,
    "mode": "interrupt",
    "edge": "falling",
    "debounce": 50,
    "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/14/state",
    "persist": true
  }'
```

### Subscribe to button events
```bash
mosquitto_sub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/io/14/state" \
  -v
```

Expected output when button is pressed:
```
esp32vault/ESP32-Vault-XXXXXXXX/io/14/state 0
esp32vault/ESP32-Vault-XXXXXXXX/io/14/state 1
```

## Example 3: Analog Sensor Reading

Read analog value from GPIO36 every 5 seconds:

### Configure the pin
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" \
  -m '{
    "pin": 36,
    "mode": "analog",
    "interval": 5000,
    "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/36/state",
    "persist": true
  }'
```

### Subscribe to sensor readings
```bash
mosquitto_sub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/io/36/state" \
  -v
```

Expected output (ADC values 0-4095):
```
esp32vault/ESP32-Vault-XXXXXXXX/io/36/state 2048
esp32vault/ESP32-Vault-XXXXXXXX/io/36/state 2051
esp32vault/ESP32-Vault-XXXXXXXX/io/36/state 2047
```

## Example 4: Relay Control with Pulse

Control a relay on GPIO12 with a 500ms pulse:

### Configure the pin
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" \
  -m '{
    "pin": 12,
    "mode": "output",
    "pulse": 500,
    "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/12/state",
    "persist": true
  }'
```

### Trigger a pulse
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/12/trigger" \
  -m '{
    "action": "pulse",
    "pulse": 500
  }'
```

## Example 5: Door Sensor with Change Detection

Monitor a door sensor on GPIO15:

### Configure the pin
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/config" \
  -m '{
    "pin": 15,
    "mode": "interrupt",
    "edge": "change",
    "debounce": 100,
    "report_topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/15/state",
    "persist": true,
    "retain": true
  }'
```

### Subscribe to door state
```bash
mosquitto_sub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/io/15/state" \
  -v
```

## Example 6: Protected Pins

Prevent accidental configuration of critical pins:

### Set exclusion list
```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/exclude" \
  -m '{
    "pins": [0, 1, 3],
    "ranges": [
      {"from": 6, "to": 11},
      {"from": 34, "to": 39}
    ],
    "persist": true
  }'
```

This excludes:
- Individual pins: 0, 1, 3 (often used for serial)
- Range 6-11: SPI flash pins (default)
- Range 34-39: Input-only pins

## Example 7: Multiple Pins Configuration

Configure multiple pins at once using a script:

```bash
#!/bin/bash
BROKER="broker.example.com"
DEVICE="ESP32-Vault-XXXXXXXX"

# Configure LED outputs
for pin in 12 13 14 15; do
    mosquitto_pub -h $BROKER \
      -t "esp32vault/$DEVICE/cmd/io/config" \
      -m "{
        \"pin\": $pin,
        \"mode\": \"output\",
        \"report_topic\": \"esp32vault/$DEVICE/io/$pin/state\",
        \"persist\": true
      }"
    sleep 0.5
done

# Configure button inputs
for pin in 16 17 18 19; do
    mosquitto_pub -h $BROKER \
      -t "esp32vault/$DEVICE/cmd/io/config" \
      -m "{
        \"pin\": $pin,
        \"mode\": \"interrupt\",
        \"edge\": \"falling\",
        \"debounce\": 50,
        \"report_topic\": \"esp32vault/$DEVICE/io/$pin/state\",
        \"persist\": true
      }"
    sleep 0.5
done
```

## Example 8: Home Automation Integration

Example Python script for Home Assistant integration:

```python
import paho.mqtt.client as mqtt
import json

BROKER = "broker.example.com"
DEVICE_ID = "ESP32-Vault-XXXXXXXX"

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    
    # Configure a light switch
    config = {
        "pin": 13,
        "mode": "output",
        "report_topic": f"esp32vault/{DEVICE_ID}/io/13/state",
        "persist": True
    }
    client.publish(
        f"esp32vault/{DEVICE_ID}/cmd/io/config",
        json.dumps(config)
    )
    
    # Subscribe to button events
    client.subscribe(f"esp32vault/{DEVICE_ID}/io/14/state")

def on_message(client, userdata, msg):
    print(f"Button state: {msg.topic} = {msg.payload.decode()}")
    
    # Toggle light when button is pressed
    if msg.payload.decode() == "0":  # Button pressed (active low)
        client.publish(
            f"esp32vault/{DEVICE_ID}/cmd/io/13/trigger",
            "toggle"
        )

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, 1883, 60)
client.loop_forever()
```

## Example 9: Node-RED Flow

Example Node-RED flow for controlling GPIO:

```json
[
  {
    "id": "mqtt_in",
    "type": "mqtt in",
    "topic": "esp32vault/ESP32-Vault-XXXXXXXX/io/+/state",
    "broker": "broker_config"
  },
  {
    "id": "trigger_out",
    "type": "mqtt out",
    "topic": "esp32vault/ESP32-Vault-XXXXXXXX/cmd/io/13/trigger",
    "broker": "broker_config"
  }
]
```

## Troubleshooting

### Pin Configuration Failed

**Problem**: Configuration rejected

**Solutions**:
1. Check if pin is in exclusion list
2. Verify pin number is valid for your ESP32 board
3. Ensure report_topic is specified
4. Check JSON syntax

### No State Reports

**Problem**: Not receiving pin state messages

**Solutions**:
1. Verify MQTT connection
2. Check report_topic is correctly configured
3. For periodic reporting, ensure interval > 0
4. For interrupt mode, ensure edge detection is configured

### Interrupt Not Triggering

**Problem**: Interrupt pin not responding

**Solutions**:
1. Verify pin supports interrupts (most GPIO pins do)
2. Check edge detection setting (rising, falling, change)
3. Increase debounce time if getting noise
4. Test with a simple on-change configuration first

## Best Practices

1. **Always set persist: true** for production configurations
2. **Use appropriate debounce values** (50-100ms for mechanical buttons)
3. **Set retain: true** for state topics you want to persist
4. **Use exclusion lists** to protect critical pins
5. **Test configurations** before making them persistent
6. **Monitor free heap** when configuring many pins
7. **Use meaningful report topics** for easier debugging

## Performance Considerations

- **Maximum pins**: Limited by available GPIO and memory
- **Event queue**: 32 events (oldest overwritten if full)
- **Debounce overhead**: ~50-100ms per event
- **MQTT publish rate**: Don't exceed broker limits
- **Worker task**: Processes events sequentially

## Security Notes

- Exclude pins used for flash and boot
- Protect MQTT broker with authentication
- Use TLS for production deployments
- Don't expose critical control pins
- Validate all external inputs
