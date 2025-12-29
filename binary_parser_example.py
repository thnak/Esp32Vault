#!/usr/bin/env python3
"""
ESP32 Vault Signal Telemetry - Binary Payload Parser Example

This script demonstrates how to parse binary payloads from ESP32 Vault
Signal Telemetry v1 system.

Dependencies:
    pip install paho-mqtt

Usage:
    python3 binary_parser_example.py --broker mqtt.example.com --mac A0B1C2D3E4F5
"""

import argparse
import struct
import paho.mqtt.client as mqtt
from datetime import datetime

class SignalTelemetryParser:
    """Parser for ESP32 Vault Signal Telemetry binary payloads"""
    
    PACKET_TYPE_RAW = 1
    PACKET_TYPE_PULSE = 2
    PACKET_TYPE_DIAG = 3
    
    def parse_raw_packet(self, payload):
        """Parse raw edge batch packet"""
        if len(payload) < 15:
            print("ERROR: Payload too short for RawPacket")
            return None
        
        # Unpack header (2 bytes)
        version, packet_type = struct.unpack('BB', payload[0:2])
        
        if packet_type != self.PACKET_TYPE_RAW:
            print(f"ERROR: Expected packet type {self.PACKET_TYPE_RAW}, got {packet_type}")
            return None
        
        # Unpack RawPacket fixed part (13 bytes)
        base_time_us, base_seq, count = struct.unpack('<QIB', payload[2:15])
        
        # Unpack edges
        edges = []
        offset = 15
        for i in range(count):
            if offset + 6 > len(payload):
                print(f"ERROR: Payload truncated at edge {i}")
                break
            
            pin, value, dt_us = struct.unpack('<BBI', payload[offset:offset+6])
            edges.append({
                'pin': pin,
                'value': value,
                'time_us': base_time_us + dt_us,
                'seq': base_seq + i,
                'dt_us': dt_us
            })
            offset += 6
        
        return {
            'version': version,
            'type': 'raw',
            'base_time_us': base_time_us,
            'base_seq': base_seq,
            'count': count,
            'edges': edges
        }
    
    def parse_pulse_packet(self, payload):
        """Parse pulse width packet"""
        if len(payload) < 23:
            print("ERROR: Payload too short for PulsePacket")
            return None
        
        # Unpack header (2 bytes)
        version, packet_type = struct.unpack('BB', payload[0:2])
        
        if packet_type != self.PACKET_TYPE_PULSE:
            print(f"ERROR: Expected packet type {self.PACKET_TYPE_PULSE}, got {packet_type}")
            return None
        
        # Unpack PulsePacket (21 bytes)
        pin, high_us, low_us, device_time_us, seq = struct.unpack('<BIIQII', payload[2:23])
        
        return {
            'version': version,
            'type': 'pulse',
            'pin': pin,
            'high_us': high_us,
            'low_us': low_us,
            'device_time_us': device_time_us,
            'seq': seq
        }
    
    def parse_diag_packet(self, payload):
        """Parse diagnostic packet"""
        if len(payload) < 14:
            print("ERROR: Payload too short for DiagPacket")
            return None
        
        # Unpack header (2 bytes)
        version, packet_type = struct.unpack('BB', payload[0:2])
        
        if packet_type != self.PACKET_TYPE_DIAG:
            print(f"ERROR: Expected packet type {self.PACKET_TYPE_DIAG}, got {packet_type}")
            return None
        
        # Unpack DiagPacket (12 bytes)
        dropped_raw, dropped_pulse, queue_depth, rmt_overflow = struct.unpack('<IIHH', payload[2:14])
        
        return {
            'version': version,
            'type': 'diag',
            'dropped_raw': dropped_raw,
            'dropped_pulse': dropped_pulse,
            'queue_depth': queue_depth,
            'rmt_overflow': rmt_overflow
        }


class SignalTelemetryClient:
    """MQTT client for ESP32 Vault Signal Telemetry"""
    
    def __init__(self, broker, port, mac_address):
        self.broker = broker
        self.port = port
        self.mac = mac_address
        self.parser = SignalTelemetryParser()
        
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
    
    def on_connect(self, client, userdata, flags, rc):
        print(f"Connected to MQTT broker with result code {rc}")
        
        # Subscribe to all signal topics
        client.subscribe("raw/#")
        client.subscribe("pulse/#")
        client.subscribe("diag")
        client.subscribe("heartbeat")
        client.subscribe(f"esp32vault/{self.mac}/status")
        
        print("Subscribed to all signal telemetry topics")
    
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload
        
        print(f"\n{'='*60}")
        print(f"Topic: {topic}")
        print(f"Payload length: {len(payload)} bytes")
        
        try:
            if topic.startswith('raw/'):
                self.handle_raw_message(topic, payload)
            elif topic.startswith('pulse/'):
                self.handle_pulse_message(topic, payload)
            elif topic == 'diag':
                self.handle_diag_message(payload)
            elif topic == 'heartbeat':
                self.handle_heartbeat_message(payload)
            elif topic.endswith('/status'):
                self.handle_status_message(payload)
            else:
                print(f"Unknown topic: {topic}")
        except Exception as e:
            print(f"ERROR parsing message: {e}")
            print(f"Raw payload (hex): {payload.hex()}")
    
    def handle_raw_message(self, topic, payload):
        """Handle raw edge batch message"""
        pin = int(topic.split('/')[1])
        data = self.parser.parse_raw_packet(payload)
        
        if data:
            print(f"Raw Edge Batch - Pin {pin}")
            print(f"  Base Time: {data['base_time_us']} us")
            print(f"  Base Seq: {data['base_seq']}")
            print(f"  Count: {data['count']}")
            print(f"  Edges:")
            for edge in data['edges']:
                timestamp = datetime.fromtimestamp(edge['time_us'] / 1000000.0)
                print(f"    Pin {edge['pin']}: {edge['value']} at {edge['time_us']}us "
                      f"(+{edge['dt_us']}us, seq {edge['seq']})")
    
    def handle_pulse_message(self, topic, payload):
        """Handle pulse width message"""
        pin = int(topic.split('/')[1])
        data = self.parser.parse_pulse_packet(payload)
        
        if data:
            print(f"Pulse Width - Pin {pin}")
            print(f"  High: {data['high_us']} us")
            print(f"  Low: {data['low_us']} us")
            print(f"  Time: {data['device_time_us']} us")
            print(f"  Seq: {data['seq']}")
            
            # Calculate frequency if possible
            if data['low_us'] > 0:
                period_us = data['high_us'] + data['low_us']
                freq_hz = 1000000.0 / period_us
                duty_cycle = (data['high_us'] / period_us) * 100
                print(f"  Frequency: {freq_hz:.2f} Hz")
                print(f"  Duty Cycle: {duty_cycle:.1f}%")
    
    def handle_diag_message(self, payload):
        """Handle diagnostic message"""
        data = self.parser.parse_diag_packet(payload)
        
        if data:
            print(f"Diagnostics")
            print(f"  Dropped Raw: {data['dropped_raw']}")
            print(f"  Dropped Pulse: {data['dropped_pulse']}")
            print(f"  Queue Depth: {data['queue_depth']}")
            print(f"  RMT Overflow: {data['rmt_overflow']}")
            
            if data['dropped_raw'] > 0:
                print(f"  WARNING: {data['dropped_raw']} raw edges were dropped!")
            if data['dropped_pulse'] > 0:
                print(f"  WARNING: {data['dropped_pulse']} pulse measurements were dropped!")
    
    def handle_heartbeat_message(self, payload):
        """Handle heartbeat message (JSON)"""
        import json
        data = json.loads(payload.decode())
        print(f"Heartbeat")
        print(f"  MAC: {data.get('mac')}")
        print(f"  Seq: {data.get('seq')}")
        print(f"  Uptime: {data.get('uptime')} seconds")
    
    def handle_status_message(self, payload):
        """Handle status message (JSON)"""
        import json
        data = json.loads(payload.decode())
        print(f"Device Status")
        print(f"  Device ID: {data.get('device_id')}")
        print(f"  Uptime: {data.get('uptime')} seconds")
        print(f"  Free Heap: {data.get('free_heap')} bytes")
        print(f"  WiFi RSSI: {data.get('wifi_rssi')} dBm")
        print(f"  Firmware: {data.get('firmware_version')}")
        if 'dropped_raw' in data:
            print(f"  Dropped Raw: {data['dropped_raw']}")
            print(f"  Dropped Pulse: {data['dropped_pulse']}")
            print(f"  Queue Depth: {data['queue_depth']}")
    
    def connect(self):
        """Connect to MQTT broker"""
        print(f"Connecting to {self.broker}:{self.port}...")
        self.client.connect(self.broker, self.port, 60)
    
    def loop_forever(self):
        """Start MQTT client loop"""
        self.client.loop_forever()


def main():
    parser = argparse.ArgumentParser(description='ESP32 Vault Signal Telemetry Binary Parser')
    parser.add_argument('--broker', required=True, help='MQTT broker address')
    parser.add_argument('--port', type=int, default=1883, help='MQTT broker port')
    parser.add_argument('--mac', required=True, help='Device MAC address (e.g., A0B1C2D3E4F5)')
    
    args = parser.parse_args()
    
    client = SignalTelemetryClient(args.broker, args.port, args.mac)
    client.connect()
    
    print("\nListening for signal telemetry data...")
    print("Press Ctrl+C to exit\n")
    
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nExiting...")


if __name__ == '__main__':
    main()
