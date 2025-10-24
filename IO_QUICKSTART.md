# IO Management Quick Start Guide

Get started with ESP32 Vault IO management in 5 minutes!

## Prerequisites

1. ESP32 board flashed with ESP32 Vault v1.1.0+
2. Device connected to WiFi
3. Device connected to MQTT broker
4. MQTT client tool (mosquitto_pub/mosquitto_sub or MQTT Explorer)
5. Know your device ID (check serial output or AP SSID)

## Step 1: Test with an LED (2 minutes)

### 1.1 Configure GPIO13 as Output

```bash
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/config" \
  -m '{
    "pin": 13,
    "mode": "output",
    "report_topic": "esp32vault/YOUR_DEVICE_ID/io/13/state"
  }'
```

Replace:
- `YOUR_BROKER` with your MQTT broker address
- `YOUR_DEVICE_ID` with your ESP32 device ID

### 1.2 Turn LED On

```bash
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/13/trigger" \
  -m "set"
```

✅ **Result**: GPIO13 goes HIGH (LED should turn on if connected)

### 1.3 Turn LED Off

```bash
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/13/trigger" \
  -m "reset"
```

✅ **Result**: GPIO13 goes LOW (LED should turn off)

### 1.4 Make LED Blink

```bash
# Toggle state
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/13/trigger" \
  -m "toggle"
```

Run this command multiple times to see the LED toggle on/off.

## Step 2: Monitor a Button (2 minutes)

### 2.1 Configure GPIO14 as Interrupt Input

```bash
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/config" \
  -m '{
    "pin": 14,
    "mode": "interrupt",
    "edge": "change",
    "debounce": 50,
    "report_topic": "esp32vault/YOUR_DEVICE_ID/io/14/state"
  }'
```

### 2.2 Subscribe to Button Events

Open a second terminal and run:

```bash
mosquitto_sub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/io/14/state" \
  -v
```

### 2.3 Test the Button

- Connect a button between GPIO14 and GND
- Press and release the button
- You should see state changes in the terminal:
  ```
  esp32vault/YOUR_DEVICE_ID/io/14/state 0
  esp32vault/YOUR_DEVICE_ID/io/14/state 1
  ```

✅ **Result**: Button presses are reported to MQTT in real-time!

## Step 3: Read Analog Sensor (1 minute)

### 3.1 Configure GPIO36 for Analog Reading

```bash
mosquitto_pub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/cmd/io/config" \
  -m '{
    "pin": 36,
    "mode": "analog",
    "interval": 5000,
    "report_topic": "esp32vault/YOUR_DEVICE_ID/io/36/state"
  }'
```

### 3.2 Subscribe to Sensor Readings

```bash
mosquitto_sub -h YOUR_BROKER \
  -t "esp32vault/YOUR_DEVICE_ID/io/36/state" \
  -v
```

✅ **Result**: Analog values (0-4095) published every 5 seconds

## Common Pin Assignments

| GPIO | Type | Notes |
|------|------|-------|
| 0 | I/O | Boot mode - use with caution |
| 1 | TX | UART0 TX - avoid if using serial |
| 2 | I/O | Built-in LED on some boards |
| 3 | RX | UART0 RX - avoid if using serial |
| 4 | I/O | Safe for general use |
| 5 | I/O | Safe for general use |
| 6-11 | Flash | **RESERVED** - Do not use |
| 12-15 | I/O | Safe for general use |
| 16-17 | I/O | Safe for general use (UART2) |
| 18-19 | I/O | Safe for general use (SPI) |
| 21-23 | I/O | Safe for general use (I2C) |
| 25-27 | I/O | Safe for general use (DAC on 25-26) |
| 32-33 | I/O | Safe for general use |
| 34-39 | Input Only | Cannot be used as output |

## Quick Reference

### Configuration Template

```json
{
  "pin": <pin_number>,
  "mode": "output|input|input_pullup|analog|interrupt",
  "edge": "rising|falling|change",
  "debounce": <milliseconds>,
  "pulse": <milliseconds>,
  "interval": <milliseconds>,
  "report_topic": "esp32vault/<device_id>/io/<pin>/state",
  "persist": true|false,
  "retain": true|false
}
```

### Trigger Actions

| Action | Effect |
|--------|--------|
| `set` | Set pin HIGH |
| `reset` | Set pin LOW |
| `pulse` | HIGH then LOW (100ms default) |
| `toggle` | Flip current state |

### Mode Options

| Mode | Description |
|------|-------------|
| `output` | Digital output |
| `input` | Digital input (floating) |
| `input_pullup` | Digital input with pull-up |
| `analog` | ADC reading (0-4095) |
| `interrupt` | Interrupt on edge detection |

## Troubleshooting

### "Pin X is excluded or reserved"

**Problem**: Pin cannot be configured

**Solution**:
- Check if pin is in range 6-11 (SPI flash - always reserved)
- Try a different pin
- Or remove pin from exclusion list (advanced)

### "report_topic is required"

**Problem**: Missing report_topic in config

**Solution**: Add `"report_topic": "esp32vault/YOUR_DEVICE_ID/io/<pin>/state"` to JSON

### No state reports

**Problem**: Not receiving MQTT messages

**Solution**:
1. Check MQTT broker connection: `mosquitto_sub -h YOUR_BROKER -t "#" -v`
2. Verify topic spelling
3. Check device logs via serial monitor
4. For interrupt mode, trigger an event
5. For analog mode, verify interval > 0

### Button bouncing

**Problem**: Getting multiple events per button press

**Solution**: Increase debounce time
```json
{
  "debounce": 100
}
```

## Next Steps

1. **Multiple Pins**: Configure several pins for a complete project
2. **Persistent Config**: Add `"persist": true` to save across reboots
3. **Home Automation**: Integrate with Home Assistant or Node-RED
4. **Advanced Features**: Check [IO_USAGE_EXAMPLES.md](IO_USAGE_EXAMPLES.md)

## Need Help?

- 📖 Full documentation: [README.md](README.md)
- 🔧 Technical details: [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md)
- 💡 More examples: [IO_USAGE_EXAMPLES.md](IO_USAGE_EXAMPLES.md)
- 🏗️ Architecture: [ARCHITECTURE.md](ARCHITECTURE.md)
- 📝 MQTT commands: [example_mqtt_commands.md](example_mqtt_commands.md)

## Example: Complete Door Sensor System

Here's a complete example combining input and output:

```bash
#!/bin/bash
BROKER="your-broker.com"
DEVICE="YOUR_DEVICE_ID"

# Configure door sensor (GPIO14)
mosquitto_pub -h $BROKER \
  -t "esp32vault/$DEVICE/cmd/io/config" \
  -m '{
    "pin": 14,
    "mode": "interrupt",
    "edge": "change",
    "debounce": 100,
    "report_topic": "home/door/state",
    "persist": true,
    "retain": true
  }'

# Configure status LED (GPIO13)
mosquitto_pub -h $BROKER \
  -t "esp32vault/$DEVICE/cmd/io/config" \
  -m '{
    "pin": 13,
    "mode": "output",
    "report_topic": "home/led/state",
    "persist": true
  }'

# Turn on LED when door opens (in a loop)
mosquitto_sub -h $BROKER -t "home/door/state" | while read msg; do
  if [[ $msg == *"0"* ]]; then
    # Door open
    mosquitto_pub -h $BROKER -t "esp32vault/$DEVICE/cmd/io/13/trigger" -m "set"
  else
    # Door closed
    mosquitto_pub -h $BROKER -t "esp32vault/$DEVICE/cmd/io/13/trigger" -m "reset"
  fi
done
```

This creates a door sensor that lights an LED when the door is open!

---

🎉 **Congratulations!** You're now ready to use ESP32 Vault's dynamic IO management system!
