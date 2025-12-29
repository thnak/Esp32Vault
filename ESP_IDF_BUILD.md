# ESP32 Vault - ESP-IDF Build Guide

This guide explains how to build and flash the ESP32 Vault Signal Telemetry v1 firmware using ESP-IDF.

## Prerequisites

### 1. Install ESP-IDF

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1.2  # or latest stable version

# Install ESP-IDF tools
./install.sh esp32

# Set up environment (add to ~/.bashrc for permanent)
. ~/esp/esp-idf/export.sh
```

For other operating systems, see: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

### 2. Verify Installation

```bash
idf.py --version
```

## Building the Firmware

### 1. Configure the Project

```bash
cd /path/to/Esp32Vault
idf.py menuconfig
```

Key settings to verify:
- **Serial flasher config** → Flash size: 4MB
- **Partition Table** → Partition Table: Two OTA partitions
- **Component config** → **MQTT Configuration** → Protocol 5 support: Enabled
- **Component config** → **Wi-Fi** → Static/Dynamic buffers

### 2. Build

```bash
idf.py build
```

This will:
- Compile all source files
- Link the firmware
- Generate the binary in `build/esp32vault.bin`

### 3. Flash to Device

```bash
# Flash and monitor (one command)
idf.py -p /dev/ttyUSB0 flash monitor

# Or flash only
idf.py -p /dev/ttyUSB0 flash

# Then monitor separately
idf.py -p /dev/ttyUSB0 monitor
```

Replace `/dev/ttyUSB0` with your actual serial port:
- Linux: Usually `/dev/ttyUSB0` or `/dev/ttyACM0`
- macOS: Usually `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- Windows: Usually `COM3`, `COM4`, etc.

### 4. Monitor Output

Press `Ctrl+]` to exit the monitor.

## Project Structure (ESP-IDF)

```
Esp32Vault/
├── CMakeLists.txt              # Root CMake file
├── sdkconfig.defaults          # Default configuration
├── main/
│   ├── CMakeLists.txt          # Main component CMake
│   ├── main.cpp                # Application entry point (app_main)
│   ├── WiFiManager.h/cpp       # WiFi management (ESP-IDF APIs)
│   ├── MQTTManager.h/cpp       # MQTT5 client (esp-mqtt)
│   ├── OTAManager.h/cpp        # OTA updates (esp_https_ota)
│   └── SignalTelemetry.h/cpp   # Signal capture system
└── build/                      # Build output (generated)
```

## Key Differences from Arduino Version

### 1. No setup() and loop()

ESP-IDF uses `app_main()` as the entry point:

```c++
extern "C" void app_main(void) {
    // Initialization code
    
    // Main loop
    while (1) {
        // Your code
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 2. Native MQTT5 Support

ESP-IDF includes `esp-mqtt` component with full MQTT5 support:
- Client ID = MAC address
- Clean start = true
- Keep alive = 60s
- Binary payloads with MQTT5 properties

### 3. FreeRTOS is Native

No need for compatibility layers - use FreeRTOS directly:
- `xTaskCreate()` for tasks
- `xQueueCreate()` for queues
- `xRingbufferCreate()` for ring buffers

### 4. Hardware APIs

Direct access to ESP32 hardware:
- GPIO via `driver/gpio.h`
- RMT via `driver/rmt_rx.h`
- Timers via `esp_timer.h`

## Configuration Options

### WiFi Configuration

Default hotspot credentials (if no saved WiFi):
- **SSID**: `EspSetup`
- **Password**: `HeLooWod`

Modify in `main/WiFiManager.cpp`:
```c++
#define DEFAULT_SSID "YourHotspot"
#define DEFAULT_PASSWORD "YourPassword"
```

### MQTT Configuration

Configured via `sdkconfig.defaults`:
```
CONFIG_MQTT_PROTOCOL_5=y
CONFIG_MQTT_TRANSPORT_SSL=y
CONFIG_MQTT_TASK_STACK_SIZE=8192
```

### Signal Telemetry

Configure in `main/SignalTelemetry.h`:
```c++
static const uint8_t MAX_BATCH_SIZE = 50;
static const uint32_t MAX_BATCH_TIME_MS = 50;
static const size_t RING_BUFFER_SIZE = 4096;
```

## Troubleshooting

### Build Errors

**"IDF_PATH not set"**
```bash
. ~/esp/esp-idf/export.sh
```

**"CMake Error"**
```bash
rm -rf build
idf.py build
```

**"Component not found"**
Ensure `sdkconfig.defaults` has all required components enabled.

### Flash Errors

**"Failed to connect"**
- Hold BOOT button while connecting
- Check serial port permissions: `sudo usermod -a -G dialout $USER` (Linux)
- Try lower baud rate: `idf.py -p PORT -b 115200 flash`

**"Chip not responding"**
- Press and hold BOOT button
- Press and release RESET button
- Release BOOT button
- Try flashing again

### Runtime Errors

**"Guru Meditation Error"**
Check the backtrace in monitor output. Common causes:
- Stack overflow (increase stack size in task creation)
- Null pointer dereference
- Division by zero

**"WiFi not connecting"**
- Verify SSID and password in NVS
- Erase NVS: `idf.py erase-flash`
- Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)

**"MQTT not connecting"**
- Verify MQTT broker configuration
- Check network connectivity
- Enable MQTT debug: `idf.py menuconfig` → Component config → ESP-MQTT → Log level

## Advanced Features

### Partition Table

To customize partitions, create `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
ota_0,    app,  ota_0,   ,        1M,
ota_1,    app,  ota_1,   ,        1M,
storage,  data, spiffs,  ,        1M,
```

Then in `CMakeLists.txt`:
```cmake
set(PARTITION_CSV_PATH ${CMAKE_SOURCE_DIR}/partitions.csv)
```

### Enable Debug Logs

In `sdkconfig.defaults`:
```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
CONFIG_MQTT_LOG_LEVEL_DEBUG=y
```

Or at runtime in code:
```c++
esp_log_level_set("*", ESP_LOG_DEBUG);
esp_log_level_set("SignalTelemetry", ESP_LOG_VERBOSE);
```

### OTA Updates

To perform OTA updates:

1. Build the firmware binary
2. Upload to HTTP/HTTPS server
3. Send MQTT command with URL

The firmware will download and flash automatically.

## Performance Optimization

### CPU Frequency

In `sdkconfig.defaults`:
```
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
```

### FreeRTOS Tick Rate

```
CONFIG_FREERTOS_HZ=1000
```

Higher tick rate = better timing precision, more overhead.

### MQTT Buffer Size

In `main/MQTTManager.cpp`:
```c++
mqtt_cfg.buffer.size = 4096;  // Increase for larger payloads
```

## Migration from Arduino

If migrating from Arduino framework:

1. Replace `Serial.println()` with `ESP_LOGI(TAG, ...)`
2. Replace `String` with `std::string`
3. Replace `millis()` with `esp_timer_get_time() / 1000`
4. Replace `delay()` with `vTaskDelay(pdMS_TO_TICKS(ms))`
5. Replace Arduino WiFi with ESP-IDF WiFi APIs
6. Replace PubSubClient with esp-mqtt

See `MIGRATION_GUIDE.md` for detailed migration instructions.

## Resources

- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **ESP-MQTT**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html
- **FreeRTOS**: https://www.freertos.org/Documentation/RTOS_book.html
- **ESP32 Technical Reference**: https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf

## Support

For issues specific to ESP-IDF build:
1. Check ESP-IDF documentation
2. Search ESP32 forums: https://esp32.com/
3. Open issue on GitHub with build logs

For ESP32 Vault specific issues:
1. Check `SIGNAL_TELEMETRY.md`
2. Review diagnostic messages
3. Open issue on GitHub repository
