#!/bin/bash
# ESP32 Vault Signal Telemetry - Quick Test Script
# 
# This script helps you quickly test the Signal Telemetry v1 system

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if required variables are set
if [ -z "$MQTT_BROKER" ]; then
    echo -e "${RED}ERROR: MQTT_BROKER environment variable not set${NC}"
    echo "Usage: export MQTT_BROKER=\"mqtt.example.com\""
    exit 1
fi

if [ -z "$DEVICE_MAC" ]; then
    echo -e "${RED}ERROR: DEVICE_MAC environment variable not set${NC}"
    echo "Usage: export DEVICE_MAC=\"A0B1C2D3E4F5\""
    exit 1
fi

echo -e "${GREEN}ESP32 Vault Signal Telemetry - Quick Test${NC}"
echo "=============================================="
echo "Broker: $MQTT_BROKER"
echo "Device MAC: $DEVICE_MAC"
echo ""

# Function to publish MQTT message
publish() {
    local topic=$1
    local message=$2
    echo -e "${YELLOW}Publishing to ${topic}${NC}"
    echo "Message: $message"
    mosquitto_pub -h "$MQTT_BROKER" -t "$topic" -m "$message"
    echo -e "${GREEN}✓ Published${NC}"
    echo ""
}

# Function to subscribe to topic
subscribe() {
    local topic=$1
    local format=${2:-"%t: %p"}
    echo -e "${YELLOW}Subscribing to ${topic}${NC}"
    echo "Press Ctrl+C to stop..."
    mosquitto_sub -h "$MQTT_BROKER" -t "$topic" -F "$format" -v
}

# Main menu
while true; do
    echo ""
    echo "Select an action:"
    echo "1. Configure pin 14 for raw edge capture"
    echo "2. Configure pin 27 for raw + pulse capture (with RMT)"
    echo "3. Remove pin configuration"
    echo "4. Subscribe to raw edge data (pin 14)"
    echo "5. Subscribe to pulse data (pin 27)"
    echo "6. Subscribe to diagnostics"
    echo "7. Subscribe to heartbeat"
    echo "8. Subscribe to device status"
    echo "9. Subscribe to ALL topics"
    echo "10. Run Python binary parser"
    echo "0. Exit"
    echo ""
    read -p "Enter choice: " choice

    case $choice in
        1)
            publish "esp32vault/${DEVICE_MAC}/cmd/signal/config" '{
  "pin": 14,
  "capture_raw": true,
  "capture_pulse": false,
  "use_rmt": false
}'
            ;;
        2)
            publish "esp32vault/${DEVICE_MAC}/cmd/signal/config" '{
  "pin": 27,
  "capture_raw": true,
  "capture_pulse": true,
  "use_rmt": true
}'
            ;;
        3)
            read -p "Enter pin number to remove: " pin
            publish "esp32vault/${DEVICE_MAC}/cmd/signal/remove" "{\"pin\": ${pin}}"
            ;;
        4)
            subscribe "raw/14" "%t: %x"
            ;;
        5)
            subscribe "pulse/27" "%t: %x"
            ;;
        6)
            subscribe "diag" "%t: %x"
            ;;
        7)
            subscribe "heartbeat"
            ;;
        8)
            subscribe "esp32vault/${DEVICE_MAC}/status"
            ;;
        9)
            echo -e "${YELLOW}Subscribing to ALL topics${NC}"
            mosquitto_sub -h "$MQTT_BROKER" -t "raw/#" -t "pulse/#" -t "diag" -t "heartbeat" -t "esp32vault/${DEVICE_MAC}/status" -v
            ;;
        10)
            if command -v python3 &> /dev/null; then
                echo -e "${GREEN}Starting Python binary parser...${NC}"
                python3 binary_parser_example.py --broker "$MQTT_BROKER" --mac "$DEVICE_MAC"
            else
                echo -e "${RED}ERROR: python3 not found${NC}"
            fi
            ;;
        0)
            echo "Exiting..."
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid choice${NC}"
            ;;
    esac
done
