# Load Cell Calibration Guide

## Why Calibration is Needed

Load cells and HX711 modules have slight variations in their characteristics. Calibration ensures accurate weight measurements by determining the relationship between raw sensor readings and actual weight.

## Quick Calibration (Recommended)

### Prerequisites
- ESP32 with HX711 and load cell properly wired
- Known weight (e.g., 100g, 500g, 1kg)
- Serial monitor or Bluetooth terminal
- The load cell should be properly mounted and stable

### Steps

1. **Upload the Code**
   - Upload the firmware to your ESP32
   - The default calibration factor is `-7050.0`

2. **Zero the Scale (Tare)**
   - Remove all weight from the load cell
   - Via Bluetooth: Send command `TARE`
   - Or: The scale automatically tares on startup

3. **Place Known Weight**
   - Place your known weight on the load cell
   - Note the displayed weight value
   - Example: You placed 500g, but it shows 350.00g

4. **Calculate Calibration Factor**
   ```
   New Factor = Old Factor × (Actual Weight / Displayed Weight)
   
   Example:
   Old Factor = -7050.0
   Actual Weight = 500g
   Displayed Weight = 350g
   
   New Factor = -7050.0 × (500 / 350)
   New Factor = -7050.0 × 1.4286
   New Factor = -10071.43
   ```

5. **Apply New Calibration Factor**
   
   **Method A - Update config.h (Permanent)**:
   ```cpp
   #define CALIBRATION_FACTOR -10071.43
   ```
   Then re-upload the firmware.
   
   **Method B - Via Bluetooth (Temporary)**:
   Send command: `CAL:-10071.43`

6. **Verify**
   - Remove and replace the known weight
   - Check if the reading matches the actual weight
   - Fine-tune if needed

## Detailed Calibration Process

### Method 1: Using Serial Monitor

1. **Get Raw Reading**
   - Temporarily modify the code to output raw values
   - In `readWeight()` function, add:
     ```cpp
     long rawValue = scale.read_average(10);
     Serial.print("Raw: ");
     Serial.println(rawValue);
     ```

2. **Record Tare Value**
   - Remove all weight from load cell
   - Note the raw reading (e.g., 8000000)

3. **Record Loaded Value**
   - Place known weight (e.g., 1000g)
   - Note the raw reading (e.g., 7930000)

4. **Calculate Calibration Factor**
   ```
   Calibration Factor = (Tare - Loaded) / Known Weight
   
   Example:
   Tare = 8000000
   Loaded = 7930000
   Known Weight = 1000g
   
   Factor = (8000000 - 7930000) / 1000
   Factor = 70000 / 1000
   Factor = 70 (or -70 depending on orientation)
   ```

### Method 2: Trial and Error

1. Start with default factor: `-7050`
2. Place a known weight
3. Adjust factor proportionally:
   - If reading is too low: increase magnitude (e.g., -7050 → -8000)
   - If reading is too high: decrease magnitude (e.g., -7050 → -6000)
4. Repeat until accurate

## Common Calibration Factors

These are typical values for reference (your actual value may differ):

| Load Cell Capacity | Typical Range      |
|-------------------|-------------------|
| 1 kg              | -1000 to -10000   |
| 5 kg              | -5000 to -50000   |
| 10 kg             | -10000 to -100000 |
| 20 kg             | -20000 to -200000 |

**Note**: The sign (+ or -) depends on load cell orientation and wiring.

## Tips for Accurate Calibration

### 1. Stable Environment
- Perform calibration in a stable, vibration-free environment
- Avoid air currents and temperature changes
- Let the system warm up for 5-10 minutes

### 2. Known Weights
- Use certified calibration weights if possible
- Household items with known weights:
  - 1 liter of water = 1000g
  - Standard dumbbells (labeled weight)
  - Food packages (check label)
  - Coins (look up specifications)

### 3. Multiple Measurements
- Test with multiple different weights
- Verify linearity across the measurement range
- The calibration should work for all weights, not just the calibration weight

### 4. Proper Placement
- Place weights in the center of the load cell
- Apply weight gradually, not suddenly
- Wait for reading to stabilize (2-3 seconds)

## Troubleshooting Calibration

### Problem: Readings Jump Around

**Solutions**:
- Increase averaging samples in code:
  ```cpp
  float weight = scale.get_units(20); // Instead of 10
  ```
- Improve wiring and grounding
- Move away from EMI sources
- Add decoupling capacitor (100nF) to HX711 VCC

### Problem: Readings Drift Over Time

**Solutions**:
- Allow warmup time (5-10 minutes)
- Check for temperature effects
- Verify mechanical mounting is secure
- Re-tare the scale periodically

### Problem: Non-linear Response

**Check**:
- Load cell might be damaged
- Exceeding maximum capacity
- Improper mounting (load cell flexing incorrectly)

### Problem: Negative Sign Issues

If all readings are inverted:
- Option 1: Flip the calibration factor sign
- Option 2: Swap A+ and A- connections on HX711
- Option 3: Rotate load cell 180 degrees

## Advanced: Multi-Point Calibration

For highest accuracy across full range:

1. Measure at multiple points:
   - 0g (tare)
   - 25% of capacity
   - 50% of capacity
   - 75% of capacity
   - 100% of capacity

2. Plot actual vs. displayed weight
3. Check for linearity
4. Adjust calibration factor if needed
5. If non-linear, consider implementing a calibration curve

## Verification

After calibration:

1. **Zero Test**: Empty load cell should read ~0g (±0.5g)
2. **Known Weight Test**: Known weight should read accurately (±1%)
3. **Repeatability Test**: Same weight should give same reading consistently
4. **Range Test**: Test across the full operating range

## Saving Calibration

### Option 1: In Code (Recommended)
Update `include/config.h`:
```cpp
#define CALIBRATION_FACTOR -7050.0  // Your calibrated value
```

### Option 2: EEPROM (Advanced)
Store calibration in EEPROM for persistence:
```cpp
#include <EEPROM.h>

// Save calibration
EEPROM.put(0, calibrationFactor);
EEPROM.commit();

// Load calibration
EEPROM.get(0, calibrationFactor);
```

## Recalibration

Recalibrate when:
- Changing load cell or HX711 module
- After mechanical modifications
- If accuracy degrades over time
- After extreme temperature changes
- Every 6-12 months for critical applications

## Quick Reference Card

```
┌─────────────────────────────────────────┐
│   QUICK CALIBRATION FORMULA             │
├─────────────────────────────────────────┤
│                                         │
│  New Factor = Old Factor ×              │
│               (Actual / Displayed)      │
│                                         │
│  Example:                               │
│  Old: -7050                             │
│  Actual: 500g                           │
│  Display: 350g                          │
│  New: -7050 × (500/350) = -10071        │
│                                         │
│  Commands:                              │
│  TARE          → Zero the scale         │
│  CAL:-10071    → Set calibration        │
│  STATUS        → Check settings         │
│                                         │
└─────────────────────────────────────────┘
```

## Support

If you're having trouble with calibration:
1. Check the [Wiring Guide](WIRING.md) first
2. Verify connections and power
3. Test with different weights
4. Open an issue on GitHub with:
   - Your load cell specifications
   - Current readings vs expected
   - Calibration factor tried

## References

- [HX711 Arduino Library Documentation](https://github.com/bogde/HX711)
- [Load Cell Theory](https://www.omega.com/en-us/resources/load-cell-types)
