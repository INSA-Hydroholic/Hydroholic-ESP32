# Hydroholic-ESP32
Repository destined to the embedded part of the project

## Overview
This ESP32 firmware integrates a load cell with the HX711 module and transmits weight data to a mobile phone via Bluetooth Classic at regular intervals.

## Features
- **Load Cell Integration**: Reads weight data using HX711 amplifier module
- **Bluetooth Connectivity**: Transmits data via Bluetooth Classic (SPP - Serial Port Profile)
- **Regular Data Transmission**: Sends weight readings at configurable intervals (default: 1 second)
- **JSON Format**: Data transmitted in JSON format for easy parsing
- **Remote Commands**: Supports commands via Bluetooth (TARE, CAL, STATUS, HELP)
- **Real-time Monitoring**: Serial output for debugging and monitoring

## Hardware Requirements
- ESP32 Development Board (any variant)
- HX711 Load Cell Amplifier Module
- Load Cell (any capacity)
- USB cable for programming
- Power supply (USB or external)

## Pin Connections
| HX711 Pin | ESP32 Pin | Description |
|-----------|-----------|-------------|
| VCC       | 3.3V/5V   | Power supply |
| GND       | GND       | Ground |
| DOUT      | GPIO 16   | Data output |
| SCK       | GPIO 4    | Serial clock |

> **Note**: You can modify the pin assignments in `include/config.h` if needed.

## Software Requirements
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- USB drivers for ESP32 (CP210x or CH340)

## Installation & Setup

### Using PlatformIO (Recommended)
1. **Install PlatformIO**:
   - Install [VSCode](https://code.visualstudio.com/)
   - Install PlatformIO IDE extension in VSCode

2. **Clone and Open Project**:
   ```bash
   git clone https://github.com/INSA-Hydroholic/Hydroholic-ESP32.git
   cd Hydroholic-ESP32
   code .
   ```

3. **Build the Project**:
   ```bash
   pio run
   ```

4. **Upload to ESP32**:
   ```bash
   pio run --target upload
   ```

5. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

### Using Arduino IDE
1. **Install Arduino IDE** and ESP32 board support
2. **Install HX711 Library**:
   - Go to Sketch → Include Library → Manage Libraries
   - Search for "HX711" by Bogdan Neagu (bogde)
   - Install version 0.7.5 or later

3. **Open the Project**:
   - Copy `src/main.cpp` content to a new Arduino sketch
   - Copy `include/config.h` content to a new tab named `config.h`

4. **Select Board**:
   - Tools → Board → ESP32 Arduino → ESP32 Dev Module

5. **Upload**:
   - Click Upload button

## Configuration
Edit `include/config.h` to customize:

```cpp
// Pin Configuration
#define HX711_DOUT_PIN 16  // Data pin
#define HX711_SCK_PIN  4   // Clock pin

// Bluetooth device name
#define BT_DEVICE_NAME "Hydroholic_ESP32"

// Data transmission interval (milliseconds)
#define TRANSMISSION_INTERVAL_MS 1000  // 1 second

// Calibration factor (adjust for your load cell)
#define CALIBRATION_FACTOR -7050.0
```

## Calibration
The load cell needs calibration for accurate measurements:

1. **Find Calibration Factor**:
   - Upload the code with a default calibration factor
   - Place a known weight on the load cell
   - Use the formula: `calibration_factor = raw_reading / known_weight`

2. **Update Configuration**:
   - Modify `CALIBRATION_FACTOR` in `include/config.h`
   - Re-upload the firmware

3. **Alternative - Runtime Calibration**:
   - Connect via Bluetooth
   - Send command: `CAL:value` (e.g., `CAL:-7050`)

## Usage

### Connecting via Bluetooth
1. **Power on ESP32** - Wait for "Hydroholic_ESP32" to appear
2. **Pair Device**:
   - On Android: Settings → Bluetooth → Pair "Hydroholic_ESP32"
   - On iOS: Use a Serial Bluetooth Terminal app
3. **Open Serial Terminal App**:
   - Android: "Serial Bluetooth Terminal" (PlayStore)
   - iOS: "BlueTerm" or similar

### Data Format
Weight data is transmitted in JSON format:
```json
{
  "weight": 123.45,
  "unit": "g",
  "timestamp": 123456,
  "device": "Hydroholic_ESP32"
}
```

### Bluetooth Commands
Send these commands from your Bluetooth terminal:
- `TARE` - Reset scale to zero
- `CAL:value` - Set calibration factor (e.g., `CAL:-7050`)
- `STATUS` - Get system status
- `HELP` - Show available commands

## Troubleshooting

### HX711 Not Found
- Check wiring connections
- Verify power supply (HX711 needs stable power)
- Try different GPIO pins

### Bluetooth Not Connecting
- Ensure Bluetooth is enabled in phone settings
- Restart ESP32 and try again
- Check if device name appears in Bluetooth scan

### Inaccurate Readings
- Recalibrate the load cell
- Ensure load cell is properly mounted
- Avoid vibrations and temperature fluctuations
- Check for loose connections

### Build Errors
- Update PlatformIO: `pio upgrade`
- Clean build: `pio run --target clean`
- Check USB cable and drivers

## Development

### Project Structure
```
Hydroholic-ESP32/
├── include/
│   └── config.h          # Configuration settings
├── src/
│   └── main.cpp          # Main application code
├── platformio.ini        # PlatformIO configuration
├── .gitignore           # Git ignore rules
└── README.md            # This file
```

### Dependencies
- **Platform**: Espressif32
- **Framework**: Arduino
- **Libraries**:
  - HX711 by bogde (^0.7.5)
  - BluetoothSerial (built-in ESP32)

## Contributing
Contributions are welcome! Please feel free to submit issues and pull requests.

## License
This project is part of the Hydroholic project.

## Support
For issues and questions, please open an issue on GitHub.
