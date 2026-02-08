# Quick Start Guide

Get your Hydroholic ESP32 load cell system up and running in 15 minutes!

## What You'll Need

- ✅ ESP32 Development Board
- ✅ HX711 Load Cell Amplifier
- ✅ Load Cell (any capacity)
- ✅ Jumper wires
- ✅ USB cable
- ✅ Computer with PlatformIO or Arduino IDE

## 5-Step Setup

### Step 1: Wire the Hardware (5 minutes)

Connect HX711 to ESP32:
```
HX711 VCC  → ESP32 3.3V (or 5V)
HX711 GND  → ESP32 GND
HX711 DT   → ESP32 GPIO 16
HX711 SCK  → ESP32 GPIO 4
```

Connect Load Cell to HX711:
```
Red Wire    → E+
Black Wire  → E-
Green/White → A+
Yellow/Blue → A-
```

> **Need help?** See detailed [Wiring Guide](docs/WIRING.md)

### Step 2: Install Software (3 minutes)

**Option A - PlatformIO (Recommended)**:
```bash
# Install VSCode and PlatformIO extension
# Then clone the repo:
git clone https://github.com/INSA-Hydroholic/Hydroholic-ESP32.git
cd Hydroholic-ESP32
code .
```

**Option B - Arduino IDE**:
- Install ESP32 board support
- Install "HX711" library by bogde
- Open `src/main.cpp` in Arduino IDE

### Step 3: Upload Code (2 minutes)

**PlatformIO**:
```bash
pio run --target upload
```

**Arduino IDE**:
- Select "ESP32 Dev Module" as board
- Click Upload button

### Step 4: Test via Serial Monitor (2 minutes)

Open serial monitor at 115200 baud:
```bash
pio device monitor
```

You should see:
```
=================================
Hydroholic ESP32 - Load Cell System
=================================

Initializing HX711 Load Cell...
HX711 initialized successfully
Taring the scale...
Load cell ready!

Initializing Bluetooth...
Bluetooth initialized. Device name: Hydroholic_ESP32

System ready!
Connect via Bluetooth to receive data

Weight: 0.00 g
Weight: 0.12 g
```

### Step 5: Connect via Bluetooth (3 minutes)

**On Android**:
1. Open Bluetooth settings
2. Pair with "Hydroholic_ESP32"
3. Install "Serial Bluetooth Terminal" app
4. Connect to "Hydroholic_ESP32"
5. See weight data streaming!

**On Computer (Python)**:
```bash
cd examples
pip install pybluez
python bluetooth_client.py
```

## What's Next?

### Calibration (Recommended)
Your scale is now working but needs calibration for accuracy:

1. Remove all weight from load cell
2. Send `TARE` command via Bluetooth
3. Place a known weight (e.g., 500g)
4. Note the displayed weight
5. Calculate: `New Factor = -7050 × (Actual / Displayed)`
6. Update `include/config.h` with new calibration factor
7. Re-upload firmware

> **Detailed guide**: [Calibration Guide](docs/CALIBRATION.md)

### Customization

Edit `include/config.h` to change:
- **Pin assignments**: Different GPIO pins
- **Bluetooth name**: Your custom device name
- **Transmission interval**: How often data is sent (ms)
- **Calibration factor**: For accurate measurements

### Build Your Application

The ESP32 sends data in JSON format:
```json
{
  "weight": 123.45,
  "unit": "g",
  "timestamp": 123456,
  "device": "Hydroholic_ESP32"
}
```

Use this data to:
- Build mobile apps
- Create data logging systems
- Integrate with IoT platforms
- Develop custom monitoring solutions

## Common Issues

### "HX711 not found"
- Check wiring connections
- Verify power supply
- Try different GPIO pins

### Bluetooth won't connect
- Ensure device is not already paired elsewhere
- Restart ESP32
- Check Bluetooth is enabled on phone

### Wrong readings
- Calibrate the load cell
- Check load cell orientation
- Verify load cell is properly mounted

## Need Help?

- 📖 [Full Documentation](README.md)
- 🔌 [Wiring Guide](docs/WIRING.md)
- 📏 [Calibration Guide](docs/CALIBRATION.md)
- 💻 [Example Code](examples/)
- 🐛 [Report Issues](https://github.com/INSA-Hydroholic/Hydroholic-ESP32/issues)

## Commands Reference

Send these via Bluetooth:
- `TARE` - Reset scale to zero
- `CAL:value` - Set calibration factor (e.g., `CAL:-7050`)
- `STATUS` - Show system status
- `HELP` - List all commands

## Success! 🎉

You now have a working ESP32 load cell system with Bluetooth connectivity!

**Enjoy your Hydroholic ESP32!**
