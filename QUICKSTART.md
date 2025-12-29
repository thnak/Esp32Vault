# Quick Start Guide - Signal Telemetry v1

Get your ESP32 Vault Signal Telemetry system up and running in minutes!

## What You'll Need

- ESP32 development board
- USB cable (for initial programming)
- Computer with ESP-IDF v5.x installed
- WiFi network (2.4GHz)
- MQTT broker (required for signal data)

## Step 1: Build and Upload

### Using ESP-IDF (Recommended)

```bash
# Clone the repository
git clone https://github.com/thnak/Esp32Vault.git
cd Esp32Vault

# Set up ESP-IDF environment (if not already done)
. $HOME/esp/esp-idf/export.sh

# Configure the project (optional)
idf.py menuconfig

# Build the project
idf.py build

# Flash to ESP32 (connect via USB)
idf.py -p /dev/ttyUSB0 flash monitor
```

**Note**: Replace `/dev/ttyUSB0` with your actual serial port (e.g., `/dev/ttyACM0` on Linux, `COM3` on Windows, `/dev/cu.usbserial-*` on macOS).

See [ESP_IDF_BUILD.md](ESP_IDF_BUILD.md) for detailed ESP-IDF build instructions.

## Step 2: Configure WiFi

### First Time Setup

1. Set up a WiFi hotspot on your laptop or mobile device:
   - **SSID**: `EspSetup`
   - **Password**: `HeLooWod`
   - **Network Type**: 2.4GHz recommended
   
2. After uploading firmware, the ESP32 will automatically connect to the `EspSetup` hotspot
   
3. Start an MQTT broker on your laptop:
   ```bash
   # Install mosquitto (if needed)
   # Ubuntu/Debian: sudo apt-get install mosquitto
   # macOS: brew install mosquitto
   
   # Start broker
   mosquitto -v
   ```

4. Find your laptop's IP on the hotspot network (typically 192.168.x.1)

5. Configure MQTT on the ESP32:
   ```bash
   # Replace with your actual device MAC address (shown in serial monitor as 12-digit hex)
   mosquitto_pub -h localhost \
     -t "esp32vault/A0B1C2D3E4F5/cmd/mqtt" \
     -m '{"server": "192.168.x.x", "port": 1883}'
   ```
   
   **Note**: The device ID is now the MAC address (e.g., `A0B1C2D3E4F5`), not `ESP32-Vault-XXXXXXXX`.

6. Configure your permanent WiFi:
   ```bash
   mosquitto_pub -h localhost \
     -t "esp32vault/A0B1C2D3E4F5/cmd/wifi" \
     -m '{"ssid": "YourWiFi", "password": "YourPassword"}'
   ```

7. ESP32 will restart and connect to your configured WiFi

### Check Connection

Monitor the serial output to see:
```
Attempting to connect to saved WiFi...
WiFi connected!
IP address: 192.168.1.XXX
```

## Step 3: Configure Signal Telemetry

### Configure a Pin for Signal Capture

Replace `A0B1C2D3E4F5` with your device MAC address:

```bash
# Configure pin 14 for raw edge capture
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 14,
    "capture_raw": true,
    "capture_pulse": false,
    "use_rmt": false
  }'
```

### Configure Pin with Pulse Width Measurement

```bash
# Configure pin 27 with RMT-based pulse measurement
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{
    "pin": 27,
    "capture_raw": true,
    "capture_pulse": true,
    "use_rmt": true
  }'
```

### Monitor Signal Data

```bash
# Subscribe to raw edge data (binary format)
mosquitto_sub -h your-broker.com -t "esp32vault/raw/14" -F "%t: %x" -v

# Subscribe to pulse width data (binary format)
mosquitto_sub -h your-broker.com -t "esp32vault/pulse/27" -F "%t: %x" -v

# Monitor diagnostics
mosquitto_sub -h your-broker.com -t "esp32vault/diag" -F "%t: %x" -v

# Monitor heartbeat (JSON format)
mosquitto_sub -h your-broker.com -t "heartbeat" -v
```

### Parse Binary Data with Python

```bash
# Use the included Python parser
python3 binary_parser_example.py \
  --broker your-broker.com \
  --mac A0B1C2D3E4F5
```

See [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md) for complete signal telemetry documentation.

## Step 4: Configure MQTT Broker (If Not Done)

### Using mosquitto_pub

Replace `A0B1C2D3E4F5` with your device MAC address:

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/mqtt" \
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
  -t "esp32vault/A0B1C2D3E4F5/cmd/mqtt" \
  -m '{
    "server": "mqtt.example.com",
    "port": 1883
  }'
```

### Verify MQTT Connection

Subscribe to status messages:

```bash
mosquitto_sub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/status" \
  -v
```

You should see status updates every 30 seconds:

```json
{
  "device_id": "A0B1C2D3E4F5",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "wifi_ssid": "YourNetwork",
  "ip_address": "192.168.1.100",
  "mqtt_connected": true,
  "firmware_version": "Signal Telemetry v1",
  "dropped_raw": 0,
  "dropped_pulse": 0,
  "queue_depth": 2
}
```

## Step 5: Perform OTA Updates

## Step 5: Perform OTA Updates

Trigger an OTA update by publishing the firmware URL:

```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/ota_update" \
  -m '{
    "version": "1.0.2",
    "url": "http://example.com/firmware.bin",
    "integrity": "sha256:your-sha256-hash"
  }'
```

**Building the firmware binary:**

```bash
# ESP-IDF
idf.py build
# Binary will be at: build/esp32vault.bin
```

Monitor OTA progress:

```bash
mosquitto_sub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/ota/status" -v
```

## Common Issues

### ESP32 Won't Connect to WiFi

**Problem**: Device keeps trying to connect

**Solutions**:
1. Verify WiFi password is correct
2. Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check router allows new devices to connect
4. Try moving ESP32 closer to router
5. Ensure the `EspSetup` hotspot is active for initial provisioning

**Reset WiFi credentials**:
```bash
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/reset_wifi" \
  -m "1"
```

### Can't Connect to Default Hotspot

**Problem**: ESP32 won't connect to `EspSetup` hotspot

**Solutions**:
1. Verify hotspot SSID is exactly `EspSetup`
2. Verify hotspot password is exactly `HeLooWod`
3. Ensure hotspot is 2.4GHz
4. Check serial monitor for connection attempts
5. Restart ESP32 with reset button

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
  -t "esp32vault/A0B1C2D3E4F5/cmd/restart" \
  -m "1"
```

Device should restart and reconnect.

### Test Signal Capture

Configure a pin and monitor data:

```bash
# Configure pin 14
mosquitto_pub -h your-broker.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/signal/config" \
  -m '{"pin":14,"capture_raw":true,"capture_pulse":false}'

# Subscribe to raw data
mosquitto_sub -h your-broker.com -t "esp32vault/raw/14" -F "%t: %x" -v
```

Connect a signal source to pin 14 (e.g., button, sensor) and observe edge changes.

### Test OTA

After configuring OTA, update the firmware:

1. Make a small change to the code
2. Build the firmware: `idf.py build`
3. Upload binary to HTTP server
4. Send OTA command with URL
5. Monitor progress via MQTT
6. Device will restart with new firmware

## Next Steps

1. **Explore Signal Telemetry**: See [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md)
2. **Understand Architecture**: Read [ARCHITECTURE.md](ARCHITECTURE.md)
3. **Parse Binary Data**: Use `binary_parser_example.py`
4. **Test Interactively**: Run `./test_signal_telemetry.sh`
5. **Migration Guide**: If upgrading from old system, see [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)
6. **ESP-IDF Details**: Review [ESP_IDF_BUILD.md](ESP_IDF_BUILD.md)

## Security Reminders

Before deploying to production:

1. ✅ Change default hotspot credentials in `main/WiFiManager.cpp` (DEFAULT_SSID and DEFAULT_PASSWORD)
2. ✅ Set OTA password (to be implemented in OTAManager)
3. ✅ Use TLS/SSL for MQTT (change port to 8883, configure certificates)
4. ✅ Use strong MQTT credentials
5. ✅ Keep firmware updated
6. ✅ Validate binary payloads on server side

## Getting Help

- **GitHub Issues**: Report bugs or request features
- **Serial Monitor**: Check for error messages
- **Documentation**: 
  - [README.md](README.md) - Main documentation
  - [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md) - Signal capture details
  - [ESP_IDF_BUILD.md](ESP_IDF_BUILD.md) - Build instructions
  - [IMPLEMENTATION_COMPLETE.md](IMPLEMENTATION_COMPLETE.md) - Full implementation summary

## System Philosophy

**Firmware = Oscilloscope | Server = Judge**

The firmware captures every signal change with microsecond precision without any filtering or interpretation. The server applies all business logic, filtering, and decision-making. This provides:

- **Maximum signal fidelity** - No edges lost to filtering
- **Complete auditability** - All data timestamped and sequenced
- **Flexibility** - Change server logic without firmware updates
- **Replay capability** - Reconstruct exact signal timeline

## Example Use Cases

### Signal Monitoring
- Capture GPIO state changes with microsecond precision
- Measure pulse widths (PWM signals, encoders, sensors)
- Monitor button presses, switches, relays
- Debug communication protocols (I2C, SPI, UART via GPIO observation)

### Industrial Applications
- Equipment monitoring with precise timing
- Sensor data capture without firmware interpretation
- Quality control signal verification
- Process timing analysis

### Development & Testing
- Oscilloscope-like signal capture
- Protocol debugging and analysis
- Timing verification
- Signal replay for testing

### Home Automation
- Precision sensor monitoring
- Switch and button state tracking
- Appliance control feedback
- Environmental monitoring with accurate timestamps

---

**Happy Building! 🚀**
