# Quick Start Guide

Get your ESP32 Vault up and running in minutes!

## What You'll Need

- ESP32 development board
- USB cable (for initial programming)
- Computer with PlatformIO installed
- WiFi network (2.4GHz)
- MQTT broker (optional, but recommended)

## Step 1: Build and Upload

### Using PlatformIO CLI

```bash
# Clone the repository
git clone https://github.com/thnak/Esp32Vault.git
cd Esp32Vault

# Build the project
pio run

# Upload to ESP32 (connect via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

### Using PlatformIO IDE (VSCode)

1. Open the project folder in VSCode
2. PlatformIO should detect the project automatically
3. Click the "Upload" button (→) in the bottom toolbar
4. Click the "Monitor" button (🔌) to view serial output

## Step 2: Configure WiFi

### First Time Setup

1. After upload, the ESP32 will start in Access Point mode
2. Look for WiFi network named: `ESP32-Vault-XXXXXXXX`
3. Connect to this network (password: `12345678`)
4. Open browser and go to: `http://192.168.4.1`
5. Enter your WiFi credentials
6. Click "Save & Connect"
7. ESP32 will restart and connect to your WiFi

### Check Connection

Monitor the serial output to see:
```
WiFi connected!
IP address: 192.168.1.XXX
```

## Step 3: Configure MQTT (Optional)

### Using mosquitto_pub

Replace `ESP32-Vault-XXXXXXXX` with your device ID:

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m '{
    "server": "mqtt.example.com",
    "port": 1883,
    "user": "your-username",
    "password": "your-password"
  }'
```

### Without Authentication

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/mqtt" \
  -m '{
    "server": "mqtt.example.com",
    "port": 1883
  }'
```

### Verify MQTT Connection

Subscribe to status messages:

```bash
mosquitto_sub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/status" \
  -v
```

You should see status updates every 30 seconds:

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

## Step 4: Enable OTA Updates

Once MQTT is configured, enable OTA for future updates:

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/ota" \
  -m "enable"
```

Now you can upload firmware over WiFi:

```bash
pio run --target upload --upload-port ESP32-Vault-XXXXXXXX.local
```

## Common Issues

### ESP32 Won't Connect to WiFi

**Problem**: Device keeps restarting in AP mode

**Solutions**:
1. Verify WiFi password is correct
2. Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check router allows new devices to connect
4. Try moving ESP32 closer to router

**Reset WiFi credentials**:
```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/reset_wifi" \
  -m "1"
```

### Can't Find AP Mode Network

**Problem**: WiFi network `ESP32-Vault-XXXXXXXX` not visible

**Solutions**:
1. Wait 30 seconds after power-on
2. Check serial monitor to confirm AP mode started
3. Look for the exact SSID (case-sensitive)
4. Restart ESP32 with reset button

### MQTT Not Connecting

**Problem**: Device shows "MQTT connection failed"

**Solutions**:
1. Verify broker address and port
2. Check broker is running and accessible
3. Verify username/password if using authentication
4. Check firewall allows MQTT port (1883)

### OTA Upload Fails

**Problem**: OTA upload times out or fails

**Solutions**:
1. Ensure ESP32 and computer are on same network
2. Check OTA was enabled via MQTT
3. Verify network allows mDNS (for .local addresses)
4. Try using IP address instead of hostname
5. Restart ESP32 and try again

## Testing Your Setup

### Test WiFi

After connecting, verify in serial monitor:
```
WiFi connected!
IP address: 192.168.1.XXX
```

### Test MQTT

Send a restart command:
```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/ESP32-Vault-XXXXXXXX/cmd/restart" \
  -m "1"
```

Device should restart and reconnect.

### Test OTA

After enabling OTA, update the code:

1. Make a small change (e.g., add a Serial.println)
2. Save the file
3. Run: `pio run --target upload --upload-port ESP32-Vault-XXXXXXXX.local`
4. Wait for upload to complete
5. Device will restart with new firmware

## Next Steps

1. **Explore MQTT Commands**: See `example_mqtt_commands.md`
2. **Understand Architecture**: Read `ARCHITECTURE.md`
3. **Customize**: Modify code for your specific needs
4. **Add Sensors**: Integrate temperature, humidity, etc.
5. **Create Dashboard**: Build monitoring interface

## Security Reminders

Before deploying to production:

1. ✅ Change default AP password in `WiFiManager.cpp`
2. ✅ Set OTA password using `otaManager.setPassword()`
3. ✅ Use TLS/SSL for MQTT (change port to 8883)
4. ✅ Use strong MQTT credentials
5. ✅ Keep firmware updated

## Getting Help

- **GitHub Issues**: Report bugs or request features
- **Serial Monitor**: Check for error messages
- **Documentation**: Review README.md and ARCHITECTURE.md

## Example Use Cases

### Home Automation
- Control lights, fans, or appliances
- Monitor sensors (temperature, motion, door contacts)
- Integrate with Home Assistant via MQTT

### Industrial Monitoring
- Equipment status monitoring
- Environmental sensors
- Alert systems

### Smart Agriculture
- Soil moisture monitoring
- Greenhouse automation
- Irrigation control

### Learning Platform
- Learn ESP32 programming
- Understand MQTT protocol
- Practice IoT development

---

**Happy Building! 🚀**
