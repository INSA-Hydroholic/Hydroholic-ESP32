# Examples

This directory contains example client applications for connecting to the Hydroholic ESP32 device.

## Bluetooth Client (Python)

A simple Python script to connect to the ESP32 via Bluetooth and receive weight data.

### Requirements

```bash
pip install pybluez
```

### Linux Setup

On Linux, you may need to install additional packages:

```bash
sudo apt-get install bluetooth libbluetooth-dev
pip install pybluez
```

### Usage

1. **Ensure ESP32 is running and Bluetooth is active**
2. **Pair the device** (optional, but recommended):
   ```bash
   bluetoothctl
   scan on
   # Wait for "Hydroholic_ESP32" to appear
   pair <MAC_ADDRESS>
   trust <MAC_ADDRESS>
   exit
   ```

3. **Run the client**:
   ```bash
   python bluetooth_client.py
   ```

### Features

- Automatic device discovery
- JSON data parsing
- Real-time weight display
- Command support (TARE, CAL, STATUS, HELP)

### Example Output

```
============================================================
Hydroholic ESP32 - Bluetooth Client
============================================================
Searching for 'Hydroholic_ESP32'...
Found: Hydroholic_ESP32 (XX:XX:XX:XX:XX:XX)

Connecting to XX:XX:XX:XX:XX:XX...
Connected successfully!

Receiving data... (Press Ctrl+C to stop)

Weight:     0.00 g  |  Time: 1234 ms
Weight:   123.45 g  |  Time: 2234 ms
Weight:   245.67 g  |  Time: 3234 ms
```

## Android App (Future)

Coming soon: Native Android application with graphical interface.

## Web Bluetooth (Future)

Coming soon: Web-based interface using Web Bluetooth API.

## Other Platforms

### macOS

PyBluez may have limitations on macOS. Consider using:
- [bleak](https://github.com/hbldh/bleak) - Cross-platform Bluetooth Low Energy library
- Native Bluetooth terminal apps from App Store

### Windows

Install PyBluez for Windows:
```bash
pip install pybluez
```

Or use Bluetooth terminal apps like:
- [RealTerm](https://sourceforge.net/projects/realterm/)
- [Tera Term](https://ttssh2.osdn.jp/)

### Mobile Apps

Recommended Bluetooth Serial Terminal apps:

**Android:**
- Serial Bluetooth Terminal (PlayStore)
- Bluetooth Terminal HC-05

**iOS:**
- BlueTerm
- BLE Terminal

## Contributing

Feel free to contribute examples for other platforms and languages!

Suggested examples:
- Node.js Bluetooth client
- Android native app
- iOS app
- Web Bluetooth interface
- Arduino to ESP32 communication
