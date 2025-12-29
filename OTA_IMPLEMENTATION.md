# OTA Implementation - HTTP/HTTPS Updates

## Overview

ESP32 Vault uses HTTP/HTTPS-based OTA (Over-The-Air) firmware updates triggered via MQTT commands. This implementation uses the ESP-IDF `esp_https_ota` component for secure and reliable firmware updates.

## Architecture

### Components

- **OTAManager** (`main/OTAManager.cpp`) - Handles OTA update process
- **MQTT Command Handler** (`main/main.cpp`) - Processes OTA commands
- **esp_https_ota** - ESP-IDF component for HTTP(S) downloads

### Update Flow

```
MQTT Broker
   │
   │ 1. Publish OTA command
   ↓
ESP32 Device
   │
   ├─→ 2. Parse command (URL, version, integrity)
   │
   ├─→ 3. Download firmware via HTTP(S)
   │   ├─→ Report progress (every 10%)
   │   └─→ Verify binary format
   │
   ├─→ 4. Flash to OTA partition
   │   └─→ Verify flash operation
   │
   └─→ 5. Reboot with new firmware
```

## MQTT Command Format

### Topic

```
esp32vault/{mac_address}/cmd/ota_update
```

Replace `{mac_address}` with your device's MAC address (e.g., `A0B1C2D3E4F5`).

### Payload

```json
{
  "version": "1.0.2",
  "url": "http://example.com/firmware.bin",
  "integrity": "sha256:abcdef1234567890..."
}
```

**Fields**:
- `version` (required): Firmware version identifier
- `url` (required): HTTP or HTTPS URL to firmware binary
- `integrity` (optional): SHA256 hash for verification (format: `sha256:hexhash`)

### Example

```bash
mosquitto_pub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/cmd/ota_update" \
  -m '{
    "version": "1.0.2",
    "url": "https://firmware.example.com/esp32vault-v1.0.2.bin",
    "integrity": "sha256:a3c5e..."
  }'
```

## Building Firmware Binary

### ESP-IDF

```bash
# Build the project
idf.py build

# Binary location
build/esp32vault.bin
```

### Arduino (PlatformIO)

```bash
# Build the project
pio run

# Binary location
.pio/build/esp32dev/firmware.bin
```

## Hosting Firmware

### Option 1: Simple HTTP Server (Development)

```bash
# Python HTTP server
cd build  # or .pio/build/esp32dev
python3 -m http.server 8080

# URL will be: http://your-ip:8080/esp32vault.bin
```

### Option 2: NGINX (Production)

```nginx
server {
    listen 80;
    server_name firmware.example.com;
    
    location /firmware/ {
        alias /var/www/firmware/;
        autoindex off;
    }
}
```

### Option 3: Cloud Storage

- **AWS S3**: Upload to S3 bucket with public read access
- **Google Cloud Storage**: Upload with public URL
- **Azure Blob Storage**: Upload with SAS token URL

## OTA Status Monitoring

### Status Topic

```
esp32vault/{mac_address}/ota/status
```

### Status Messages

**Starting**:
```json
{
  "status": "starting"
}
```

**Progress** (published periodically):
```json
{
  "status": "downloading",
  "progress": 45
}
```

**Success**:
```json
{
  "status": "success"
}
```

**Error**:
```json
{
  "status": "error",
  "message": "Download failed"
}
```

### Monitoring Example

```bash
# Subscribe to OTA status
mosquitto_sub -h broker.example.com \
  -t "esp32vault/A0B1C2D3E4F5/ota/status" \
  -v
```

## Security Considerations

### HTTPS vs HTTP

**HTTPS (Recommended for Production)**:
```json
{
  "url": "https://firmware.example.com/esp32vault-v1.0.2.bin"
}
```

Benefits:
- Encrypted transfer
- Certificate verification
- Protection against MITM attacks

**HTTP (Development Only)**:
```json
{
  "url": "http://192.168.1.100:8080/firmware.bin"
}
```

### Integrity Verification

Include SHA256 hash for additional security:

```bash
# Calculate SHA256
sha256sum build/esp32vault.bin

# Use in OTA command
{
  "url": "https://...",
  "integrity": "sha256:calculated-hash-here"
}
```

**Note**: Full SHA256 verification is marked for future enhancement. Currently, ESP-IDF provides built-in binary format verification.

### Access Control

1. **Authentication**: Use HTTP Basic Auth or API keys on firmware server
2. **Token-based**: Generate temporary download tokens
3. **IP Whitelist**: Restrict firmware downloads to known IPs
4. **Time-limited URLs**: Use signed URLs with expiration

## Partition Table

ESP32 uses two OTA partitions for safe updates:

```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 1M
ota_0,    app,  ota_0,   ,        1M
ota_1,    app,  ota_1,   ,        1M
```

**How it works**:
1. Device boots from `ota_0` (or `ota_1`)
2. OTA downloads new firmware to inactive partition
3. On success, marks new partition as bootable
4. Device reboots into new firmware
5. If new firmware fails, rolls back to previous partition

## Troubleshooting

### OTA Fails to Start

**Problem**: Device doesn't respond to OTA command

**Solutions**:
1. Verify MQTT connection: Check `esp32vault/{mac}/status`
2. Verify topic name: Must match device MAC address
3. Check JSON format: Use JSON validator
4. Monitor serial output for error messages

### Download Fails

**Problem**: OTA starts but download fails

**Solutions**:
1. Verify URL is accessible from device's network
2. Check firewall allows outgoing HTTP/HTTPS
3. Ensure firmware server is running
4. Test URL in browser or with `curl`
5. Check serial output for HTTP error codes

### Flash Verification Fails

**Problem**: Download succeeds but flash fails

**Solutions**:
1. Verify binary is valid ESP32 firmware
2. Check binary size fits in OTA partition (< 1MB default)
3. Ensure binary is built for correct ESP32 variant
4. Verify partition table is correct

### Device Doesn't Reboot

**Problem**: OTA completes but device doesn't reboot

**Solutions**:
1. Check serial output for reboot messages
2. Manually restart device
3. Verify new firmware is bootable
4. Check if rollback occurred (serial logs)

### Rollback to Previous Version

If new firmware doesn't work properly:

1. Device will automatically rollback after 3 boot failures
2. Or manually trigger rollback:

```cpp
// In your app_main() if firmware is bad
esp_ota_mark_app_invalid();
esp_restart();
```

## Best Practices

### Development

1. **Test locally first**: Use HTTP server on local network
2. **Incremental updates**: Test small changes before major updates
3. **Version tracking**: Always increment version number
4. **Backup**: Keep previous firmware versions

### Production

1. **Use HTTPS**: Always use secure connections
2. **Staged rollout**: Update a few devices first
3. **Monitor status**: Watch OTA status topic
4. **Integrity checks**: Always include SHA256 hash
5. **Rollback plan**: Keep previous firmware available
6. **Testing**: Test OTA process thoroughly before production

### Security

1. **Authentication**: Require credentials for firmware downloads
2. **Authorization**: Validate device before allowing download
3. **Encryption**: Use HTTPS for all firmware transfers
4. **Signing**: Sign firmware binaries (future enhancement)
5. **Audit logs**: Log all OTA attempts and results

## Integration Examples

### Python Script

```python
import paho.mqtt.client as mqtt
import json
import hashlib

def calculate_sha256(filename):
    sha256_hash = hashlib.sha256()
    with open(filename, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def trigger_ota(broker, mac, version, url, firmware_file):
    # Calculate integrity hash
    sha256 = calculate_sha256(firmware_file)
    
    # Create OTA command
    command = {
        "version": version,
        "url": url,
        "integrity": f"sha256:{sha256}"
    }
    
    # Publish to device
    client = mqtt.Client()
    client.connect(broker, 1883)
    topic = f"esp32vault/{mac}/cmd/ota_update"
    client.publish(topic, json.dumps(command))
    client.disconnect()
    
    print(f"OTA triggered for {mac}: {version}")

# Usage
trigger_ota(
    broker="mqtt.example.com",
    mac="A0B1C2D3E4F5",
    version="1.0.2",
    url="https://firmware.example.com/esp32vault-v1.0.2.bin",
    firmware_file="build/esp32vault.bin"
)
```

### Node.js Script

```javascript
const mqtt = require('mqtt');
const fs = require('fs');
const crypto = require('crypto');

function calculateSHA256(filename) {
    const data = fs.readFileSync(filename);
    return crypto.createHash('sha256').update(data).digest('hex');
}

function triggerOTA(broker, mac, version, url, firmwareFile) {
    const client = mqtt.connect(`mqtt://${broker}`);
    
    client.on('connect', () => {
        const sha256 = calculateSHA256(firmwareFile);
        const command = {
            version: version,
            url: url,
            integrity: `sha256:${sha256}`
        };
        
        const topic = `esp32vault/${mac}/cmd/ota_update`;
        client.publish(topic, JSON.stringify(command));
        console.log(`OTA triggered for ${mac}: ${version}`);
        client.end();
    });
}

// Usage
triggerOTA(
    'mqtt.example.com',
    'A0B1C2D3E4F5',
    '1.0.2',
    'https://firmware.example.com/esp32vault-v1.0.2.bin',
    'build/esp32vault.bin'
);
```

## Resources

- **ESP-IDF OTA Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- **esp_https_ota**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html
- **Partition Tables**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html

## See Also

- [ESP_IDF_BUILD.md](ESP_IDF_BUILD.md) - Build instructions
- [README.md](README.md) - Main documentation
- [SIGNAL_TELEMETRY.md](SIGNAL_TELEMETRY.md) - Signal capture system
