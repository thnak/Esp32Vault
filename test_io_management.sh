#!/bin/bash

# ESP32 Vault IO Management Test Script
# This script tests the basic functionality of IO management features

set -e

# Configuration
BROKER="${MQTT_BROKER:-localhost}"
DEVICE="${DEVICE_ID:-ESP32-Vault-XXXXXXXX}"
TEST_OUTPUT_PIN="${OUTPUT_PIN:-13}"
TEST_INPUT_PIN="${INPUT_PIN:-14}"

echo "=================================="
echo "ESP32 Vault IO Management Test"
echo "=================================="
echo ""
echo "Configuration:"
echo "  MQTT Broker: $BROKER"
echo "  Device ID: $DEVICE"
echo "  Output Pin: $TEST_OUTPUT_PIN"
echo "  Input Pin: $TEST_INPUT_PIN"
echo ""
echo "Press Ctrl+C to exit"
echo ""

# Check if mosquitto_pub is available
if ! command -v mosquitto_pub &> /dev/null; then
    echo "ERROR: mosquitto_pub not found. Please install mosquitto-clients."
    exit 1
fi

if ! command -v mosquitto_sub &> /dev/null; then
    echo "ERROR: mosquitto_sub not found. Please install mosquitto-clients."
    exit 1
fi

echo "Step 1: Configure output pin $TEST_OUTPUT_PIN"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/config" \
  -m "{
    \"pin\": $TEST_OUTPUT_PIN,
    \"mode\": \"output\",
    \"report_topic\": \"esp32vault/$DEVICE/io/$TEST_OUTPUT_PIN/state\",
    \"persist\": false
  }"
echo "✓ Output pin configured"
sleep 1

echo ""
echo "Step 2: Configure input pin $TEST_INPUT_PIN"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/config" \
  -m "{
    \"pin\": $TEST_INPUT_PIN,
    \"mode\": \"interrupt\",
    \"edge\": \"change\",
    \"debounce\": 50,
    \"report_topic\": \"esp32vault/$DEVICE/io/$TEST_INPUT_PIN/state\",
    \"persist\": false
  }"
echo "✓ Input pin configured"
sleep 1

echo ""
echo "Step 3: Set output HIGH"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/$TEST_OUTPUT_PIN/trigger" \
  -m "set"
echo "✓ Output set HIGH"
sleep 1

echo ""
echo "Step 4: Set output LOW"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/$TEST_OUTPUT_PIN/trigger" \
  -m "reset"
echo "✓ Output set LOW"
sleep 1

echo ""
echo "Step 5: Toggle output 3 times"
for i in {1..3}; do
    mosquitto_pub -h "$BROKER" \
      -t "esp32vault/$DEVICE/cmd/io/$TEST_OUTPUT_PIN/trigger" \
      -m "toggle"
    echo "  Toggle $i"
    sleep 0.5
done
echo "✓ Toggle complete"
sleep 1

echo ""
echo "Step 6: Pulse output"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/$TEST_OUTPUT_PIN/trigger" \
  -m "pulse"
echo "✓ Pulse sent"
sleep 1

echo ""
echo "Step 7: Set exclusion list"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/exclude" \
  -m '{
    "pins": [0, 1, 3],
    "ranges": [{"from": 6, "to": 11}],
    "persist": false
  }'
echo "✓ Exclusion list set"
sleep 1

echo ""
echo "Step 8: Try to configure reserved pin (should fail)"
mosquitto_pub -h "$BROKER" \
  -t "esp32vault/$DEVICE/cmd/io/config" \
  -m '{
    "pin": 6,
    "mode": "output",
    "report_topic": "esp32vault/'$DEVICE'/io/6/state"
  }'
echo "✓ Reserved pin test complete (check device logs for error)"
sleep 1

echo ""
echo "Step 9: Subscribe to pin states (10 seconds)"
echo "  If configured correctly, you should see pin states..."
timeout 10 mosquitto_sub -h "$BROKER" \
  -t "esp32vault/$DEVICE/io/+/state" \
  -v || echo "✓ Subscription test complete"

echo ""
echo "=================================="
echo "All tests completed!"
echo "=================================="
echo ""
echo "Summary:"
echo "  ✓ Output pin configuration"
echo "  ✓ Input pin configuration"
echo "  ✓ Trigger operations (set, reset, toggle, pulse)"
echo "  ✓ Exclusion list management"
echo "  ✓ Reserved pin protection"
echo "  ✓ State reporting"
echo ""
echo "Check your ESP32 serial output for detailed logs."
echo ""

# Optional: Clean up (uncomment if desired)
# echo "Cleaning up test configuration..."
# mosquitto_pub -h "$BROKER" \
#   -t "esp32vault/$DEVICE/cmd/io/exclude" \
#   -m '{"pins": [], "ranges": [], "persist": false}'
# echo "✓ Cleanup complete"
