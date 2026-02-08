#!/usr/bin/env python3
"""
Bluetooth Client for Hydroholic ESP32
This script connects to the ESP32 via Bluetooth and receives weight data.

Requirements:
    pip install pybluez

Usage:
    python bluetooth_client.py

Note: On Linux, you may need to pair the device first using bluetoothctl
"""

import bluetooth
import json
import sys
import time

# Configuration
DEVICE_NAME = "Hydroholic_ESP32"
DEVICE_ADDRESS = None  # Will be discovered automatically

def find_device(target_name):
    """
    Find Bluetooth device by name
    """
    print(f"Searching for '{target_name}'...")
    nearby_devices = bluetooth.discover_devices(duration=8, lookup_names=True)
    
    for addr, name in nearby_devices:
        print(f"Found: {name} ({addr})")
        if name == target_name:
            return addr
    
    return None

def connect_to_device(address):
    """
    Connect to Bluetooth device
    """
    print(f"\nConnecting to {address}...")
    
    # Create Bluetooth socket
    sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    
    try:
        # Connect to the device on port 1 (standard RFCOMM port)
        sock.connect((address, 1))
        print("Connected successfully!")
        return sock
    except bluetooth.btcommon.BluetoothError as e:
        print(f"Connection failed: {e}")
        return None

def send_command(sock, command):
    """
    Send command to ESP32
    """
    try:
        sock.send(f"{command}\n".encode())
        print(f"Sent: {command}")
    except Exception as e:
        print(f"Error sending command: {e}")

def receive_data(sock):
    """
    Receive and process data from ESP32
    """
    print("\nReceiving data... (Press Ctrl+C to stop)\n")
    
    buffer = ""
    
    try:
        while True:
            # Receive data
            data = sock.recv(1024).decode()
            
            if not data:
                print("Connection closed by device")
                break
            
            buffer += data
            
            # Process complete lines
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip()
                
                if not line:
                    continue
                
                # Try to parse as JSON
                if line.startswith('{'):
                    try:
                        json_data = json.loads(line)
                        weight = json_data.get('weight', 0)
                        unit = json_data.get('unit', 'g')
                        timestamp = json_data.get('timestamp', 0)
                        
                        print(f"Weight: {weight:8.2f} {unit}  |  Time: {timestamp} ms")
                    except json.JSONDecodeError:
                        print(f"Data: {line}")
                else:
                    print(f"Info: {line}")
    
    except KeyboardInterrupt:
        print("\n\nStopped by user")
    except Exception as e:
        print(f"\nError: {e}")

def main():
    """
    Main function
    """
    print("=" * 60)
    print("Hydroholic ESP32 - Bluetooth Client")
    print("=" * 60)
    
    # Find device
    device_address = find_device(DEVICE_NAME)
    
    if not device_address:
        print(f"\nDevice '{DEVICE_NAME}' not found!")
        print("Make sure:")
        print("  1. ESP32 is powered on")
        print("  2. Bluetooth is enabled on your computer")
        print("  3. Device is not already connected")
        sys.exit(1)
    
    # Connect to device
    sock = connect_to_device(device_address)
    
    if not sock:
        print("Failed to connect!")
        sys.exit(1)
    
    # Optional: Send a command
    # Uncomment to send commands on connect
    # send_command(sock, "STATUS")
    
    # Receive data
    receive_data(sock)
    
    # Clean up
    print("\nClosing connection...")
    sock.close()
    print("Done!")

if __name__ == "__main__":
    main()
