# Wiring Guide

## Components Required

1. **ESP32 Development Board** (any variant)
2. **HX711 Load Cell Amplifier Module**
3. **Load Cell** (any capacity: 1kg, 5kg, 10kg, 20kg, etc.)
4. **Jumper Wires** (Female-to-Female recommended)
5. **USB Cable** for programming and power
6. **Breadboard** (optional, for prototyping)

## Pin Connections

### ESP32 to HX711

| HX711 Pin | Wire Color (typical) | ESP32 Pin | Description          |
|-----------|---------------------|-----------|----------------------|
| VCC       | Red                 | 3.3V or 5V| Power supply        |
| GND       | Black               | GND       | Ground              |
| DT (DOUT) | Green               | GPIO 16   | Data output         |
| SCK       | White/Yellow        | GPIO 4    | Serial clock        |

### Load Cell to HX711

Most load cells have 4 wires with standard color coding:

| Load Cell Wire | Color (typical) | HX711 Pin | Description    |
|----------------|----------------|-----------|----------------|
| E+             | Red            | E+        | Excitation +   |
| E-             | Black          | E-        | Excitation -   |
| A+ (S+)        | White/Green    | A+        | Signal +       |
| A- (S-)        | Yellow/Blue    | A-        | Signal -       |

**Note**: Wire colors may vary by manufacturer. Always check your load cell's datasheet.

## Wiring Diagram (ASCII)

```
                    ESP32
                 +---------+
                 |         |
                 |  3.3V   |----[Red]-----> VCC
                 |         |                 |
                 |  GND    |----[Black]----> GND
                 |         |                 |
                 | GPIO 16 |----[Green]----> DT    HX711
                 |         |                 |    Module
                 | GPIO 4  |----[White]----> SCK   +-----+
                 |         |                 |     |     |
                 |   USB   |<--USB Cable     |     | E+  |<--[Red]----+
                 |         |                 |     | E-  |<--[Black]--+
                 +---------+                 |     | A+  |<--[Green]--+-- Load Cell
                                             |     | A-  |<--[White]--+
                                             +-----+                  
```

## Assembly Instructions

### Step 1: Connect HX711 to ESP32

1. **Power Connection**:
   - Connect HX711 **VCC** to ESP32 **3.3V** (or 5V if your module requires it)
   - Connect HX711 **GND** to ESP32 **GND**

2. **Data Connections**:
   - Connect HX711 **DT** to ESP32 **GPIO 16**
   - Connect HX711 **SCK** to ESP32 **GPIO 4**

### Step 2: Connect Load Cell to HX711

1. Identify your load cell wires (check datasheet or use multimeter)
2. Connect the 4 wires to the HX711 screw terminals:
   - **E+** (Excitation +): Usually RED
   - **E-** (Excitation -): Usually BLACK
   - **A+** (Signal +): Usually WHITE or GREEN
   - **A-** (Signal -): Usually YELLOW or BLUE

### Step 3: Load Cell Installation

1. **Mount the load cell** securely on a stable surface
2. **Ensure proper alignment** - the arrow on the load cell shows the direction of force
3. **Leave the sensing area free** - don't clamp or restrict the measurement section
4. **Apply load from above** - place objects on top of the load cell

## Important Notes

### Power Supply
- HX711 can work with 3.3V or 5V (check your module specs)
- ESP32 provides both 3.3V and 5V outputs
- Using 5V may provide better signal quality but verify your module supports it

### GPIO Pin Selection
- The default pins (GPIO 16 and GPIO 4) are safe choices
- You can use other GPIO pins if needed - just update `config.h`
- Avoid these pins: GPIO 0, 2, 12, 15 (boot pins), GPIO 6-11 (flash pins)

### Wire Quality
- Use good quality jumper wires
- Keep wires as short as possible to reduce noise
- Twist data and clock wires together if possible
- Keep power and signal wires separated

### Load Cell Orientation
- The load cell has directional sensitivity
- Most load cells have an arrow indicating the force direction
- Incorrect orientation will give negative or inconsistent readings

### Grounding
- Ensure good ground connection
- All grounds (ESP32, HX711, Load Cell) should be common
- Poor grounding can cause noise and drift

## Troubleshooting Connections

### Problem: "HX711 not found" Error

**Check**:
1. Verify all connections are secure
2. Check power LED on HX711 (if present)
3. Measure voltage at VCC pin (should be 3.3V or 5V)
4. Try swapping DT and SCK connections
5. Test with different GPIO pins

### Problem: Reading Always Zero

**Check**:
1. Load cell connections to HX711
2. Try swapping A+ and A- wires
3. Verify load cell is not damaged (measure resistance)
4. Check that load cell is properly mounted

### Problem: Negative or Inverted Readings

**Solution**:
1. Swap A+ and A- connections, OR
2. Flip the load cell orientation, OR
3. Change calibration factor sign in config.h

### Problem: Noisy or Unstable Readings

**Solutions**:
1. Use shorter, higher quality wires
2. Add a 100nF capacitor between VCC and GND
3. Keep away from EMI sources (motors, WiFi, etc.)
4. Ensure stable power supply
5. Shield the signal wires

## Testing the Connections

After wiring, upload the code and check Serial Monitor:

1. You should see: `"HX711 initialized successfully"`
2. Weight readings should appear every second
3. Placing weight on the load cell should change the readings
4. Readings should be stable (±0.1 units)

## Next Steps

After successful wiring:
1. Proceed to [Calibration Guide](CALIBRATION.md)
2. Test Bluetooth connectivity
3. Build your application!

## Safety Warnings

⚠️ **WARNING**:
- Don't exceed the load cell's maximum capacity
- Don't drop weights on the load cell
- Handle the load cell carefully - internal strain gauges are delicate
- Ensure secure mounting to prevent accidents
- Use appropriate power supply ratings

## Reference

For more information:
- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [HX711 Datasheet](https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf)
