# PM2.5 Sensor Troubleshooting Guide

## Issue Description

The DFRobot PM2.5 sensor (SEN0460) was initializing successfully but consistently returning zero readings for all particle concentration measurements (PM1.0, PM2.5, and PM10 in μg/m³).

**UPDATE**: After testing, discovered that `gainParticleConcentration_ugm3()` returns zeros, but `gainParticleNum_Every0_1L()` works correctly. This appears to be a firmware limitation with the concentration calculation registers.

### Symptoms
- Sensor initialization: ✅ Successful (I2C communication working)
- Sensor readings: ❌ All zeros
- Other sensors (ENS160, CO, O2): ✅ Working normally

```
PM2.5 (0x19): OK
PM1.0: 0 | PM2.5: 0 | PM10: 0 μg/m³
```

## Root Cause Analysis

After comprehensive testing with library examples, the root cause was identified:

### **Sensor Firmware Limitation - Concentration Registers Not Populated**

The sensor's concentration calculation registers (`gainParticleConcentration_ugm3()`) consistently return zeros, while the particle count registers (`gainParticleNum_Every0_1L()`) work perfectly. This was confirmed by:

1. ✅ **Particle number example works** - Returns valid particle counts
2. ❌ **Concentration example fails** - Returns all zeros for μg/m³ readings
3. ✅ **Sensor is functional** - I2C communication and particle detection working

### Secondary Issues Addressed

1. **Missing Sensor Wake-Up Call** - Sensor supports low-power mode and needs explicit `awake()` call
2. **Insufficient Warm-Up Time** - Laser sensors need 30-60 seconds to stabilize
3. **No Data Validation** - Original code didn't check for zero readings or verify firmware version

## Solution Implemented

### Code Changes in `src/main.cpp`

#### 1. **Switched from Concentration to Particle Counts** (Primary Fix)

Changed from using `gainParticleConcentration_ugm3()` to `gainParticleNum_Every0_1L()`:

**Before (didn't work):**
```cpp
uint16_t pm1 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM1_0_STANDARD);
uint16_t pm25 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM2_5_STANDARD);
uint16_t pm10 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM10_STANDARD);
// Always returned 0, 0, 0
```

**After (works):**
```cpp
uint16_t pm1_count = pm25Sensor.gainParticleNum_Every0_1L(PARTICLENUM_1_0_UM_EVERY0_1L_AIR);
uint16_t pm25_count = pm25Sensor.gainParticleNum_Every0_1L(PARTICLENUM_2_5_UM_EVERY0_1L_AIR);
uint16_t pm10_count = pm25Sensor.gainParticleNum_Every0_1L(PARTICLENUM_10_UM_EVERY0_1L_AIR);
// Returns actual particle counts
```

**Unit Changed:** μg/m³ → particles/0.1L

#### 2. **Updated Home Assistant Discovery** (Lines 229-244)

Updated MQTT discovery to reflect particle counts instead of concentration:

```cpp
publishSensorDiscovery("Air Quality PM1.0 Count", "esp32_aq_pm1",
                      "homeassistant/sensor/esp32_air_quality_pm1/state",
                      "particles/0.1L", "", "mdi:air-filter");
```

#### 3. **Enhanced Sensor Initialization** (Lines 170-201)

Added initialization improvements:
- `pm25Sensor.awake()` - Wake sensor from potential sleep mode
- `pm25Sensor.gainVersion()` - Verify sensor firmware version
- 30-second warm-up period with countdown
- Detailed status logging

```cpp
if (pm25Available) {
  pm25Sensor.awake();
  delay(100);
  
  uint8_t version = pm25Sensor.gainVersion();
  Serial.print("OK (v");
  Serial.print(version);
  Serial.println(")");
  
  Serial.println("  Warming up PM2.5 sensor (30s)...");
  for (int i = 30; i > 0; i--) {
    Serial.print("  ");
    Serial.print(i);
    Serial.println("s remaining");
    delay(1000);
  }
  Serial.println("  Warm-up complete!");
}
```

#### 4. **Enhanced Diagnostic Logging** (Lines 323-326)

Added zero-reading detection:
```cpp
if (pm1_count == 0 && pm25_count == 0 && pm10_count == 0) {
  Serial.print(" [⚠️ All zeros - Very clean air or sensor issue]");
}
```

## Expected Behavior After Fix

### During Startup
```
Initializing sensors...
-------------------------
PM2.5 (0x19): OK (v15)
  Warming up PM2.5 sensor (30s)...
  30s remaining
  29s remaining
  ...
  1s remaining
  Warm-up complete!
```

### During Normal Operation
After warm-up, the sensor will provide **particle counts** instead of concentration:

```
--- Reading Sensors ---
PM1.0: 1234 | PM2.5: 856 | PM10: 342 particles/0.1L
TVOC: 28 ppb | eCO2: 410 ppm | AQI: 1
CO: 0.00 ppm
O2: 18.80%
```

Or if measuring very clean air:
```
--- Reading Sensors ---
PM1.0: 0 | PM2.5: 0 | PM10: 0 particles/0.1L [⚠️ All zeros - Very clean air or sensor issue]
```

### Understanding Particle Counts vs Concentration

**Particle Count** (what we're measuring): Number of particles detected per 0.1 liters of air
- PM1.0: Particles with diameter ≤ 1.0 μm
- PM2.5: Particles with diameter ≤ 2.5 μm
- PM10: Particles with diameter ≤ 10 μm

**Concentration** (not working on this sensor): Mass concentration in μg/m³
- Requires sensor to calculate particle mass based on size distribution
- This firmware feature appears non-functional on this particular sensor

**Note**: Particle counts are valid air quality indicators. Higher counts = more particles in air.

## Testing Recommendations

### 1. Upload and Monitor
Upload the updated firmware and monitor the serial output during the 30-second warm-up period.

### 2. Verify Readings After Warm-Up
After warm-up completes, check if the sensor starts reporting non-zero values.

### 3. Generate Particulates for Testing
If readings are still zero after warm-up:
- Blow air near the sensor
- Light incense or a match nearby (safely!)
- Use an aerosol spray at a distance
- Move to a different room with more dust

### 4. Check for Hardware Issues
If still reading zeros after testing:

#### Verify I2C Connection
- Check wiring: SDA, SCL, VCC (5V), GND
- Ensure no loose connections
- Verify pull-up resistors on I2C lines (usually built into ESP32)

#### Test with Library Example
Upload the DFRobot example to isolate the issue:
```
.pio/libdeps/esp32dev/DFRobot_AirQualitySensor/examples/gainconcentration/gainConcentration.ino
```

#### Check Power Supply
PM2.5 sensors can draw significant current for the laser. Ensure:
- Adequate power supply (500mA minimum recommended)
- Stable voltage (not from weak USB port)
- No voltage drops during operation

## Understanding Zero Readings

Zero readings can be legitimate in these scenarios:

### Valid Scenarios (Not a Problem)
1. **Very Clean Air** - Indoor environments with good air filtration
2. **New Environment** - Recently cleaned room
3. **Low Activity** - No cooking, smoking, or movement

### Problem Scenarios (Hardware Issue)
1. **Sensor Malfunction** - Laser not functioning
2. **Poor Connection** - Intermittent I2C communication
3. **Wrong Mode** - Sensor stuck in sleep/test mode
4. **Damaged Sensor** - Physical damage to optical chamber

## Technical Details

### DFRobot PM2.5 Sensor (SEN0460)
- **Principle**: Laser scattering detection
- **I2C Address**: 0x19
- **Measurements**: PM1.0, PM2.5, PM10 (μg/m³)
- **Detection Method**: Counts particles and calculates concentration
- **Warm-Up Time**: 30-60 seconds recommended
- **Power Mode**: Supports sleep mode via low-power command

### Library Functions Used
- `begin()` - Initialize I2C communication
- `awake()` - Wake sensor from sleep mode
- `gainVersion()` - Read firmware version (diagnostic)
- `gainParticleConcentration_ugm3(type)` - Read PM concentration

## Additional Resources

- [DFRobot PM2.5 Sensor Wiki](https://wiki.dfrobot.com/Gravity_PM2.5_Air_Quality_Sensor_SKU_SEN0460)
- [Library GitHub](https://github.com/dfrobot/DFRobot_AirQualitySensor)
- Sensor Datasheet: Available from DFRobot product page

## Troubleshooting Checklist

- [ ] Sensor initializes successfully
- [ ] Firmware version displays during init
- [ ] 30-second warm-up completes
- [ ] Tested in different environments
- [ ] Verified I2C wiring (SDA, SCL, VCC, GND)
- [ ] Checked power supply adequacy
- [ ] Tested library example independently
- [ ] Generated particles near sensor for testing
- [ ] Reviewed serial output for error messages

## When to Suspect Hardware Failure

Contact DFRobot support or consider sensor replacement if:
- ✅ All code fixes implemented
- ✅ Wiring verified correct
- ✅ Power supply adequate
- ✅ Library example also fails
- ✅ Sensor returns zeros even with smoke/particles present
- ✅ Firmware version reads as 0 or 255
- ⚠️ Sensor might be defective

## Impact on Home Assistant and Rainmeter

### Home Assistant
The sensor entities will now report particle counts instead of μg/m³:
- **Entity**: `sensor.air_quality_pm2_5`
- **Unit**: `particles/0.1L` (instead of `μg/m³`)
- **Value**: Particle count (higher = more particles in air)

You may need to delete and re-discover the sensors in Home Assistant for the new unit to display correctly.

### Rainmeter Widget
The Rainmeter widget will need to be updated to:
- Display particle counts instead of concentration
- Update threshold values for particle counts
- Adjust labels and units

**Typical particle count ranges:**
- **Very Clean**: 0-500 particles/0.1L
- **Clean**: 500-1000 particles/0.1L
- **Moderate**: 1000-5000 particles/0.1L
- **Poor**: 5000-10000 particles/0.1L
- **Very Poor**: >10000 particles/0.1L

## Alternative Solution: Calculate Concentration

If you need μg/m³ values for compatibility, you could implement a rough conversion:

```cpp
// Approximate conversion (varies by particle density)
// This is an estimation and not scientifically accurate
float pm25_approx_ugm3 = pm25_count * 0.01;  // Rough multiplier
```

However, particle counts are scientifically valid air quality indicators and may be more accurate than estimated concentrations.

## Version History

- **v1.1** (2025-12-14) - Updated after testing
  - **Identified root cause**: Concentration registers return zeros, particle count registers work
  - **Switched to particle counts** instead of concentration
  - Updated Home Assistant discovery for new units
  - Added conversion notes and particle count ranges
  
- **v1.0** (2025-12-14) - Initial troubleshooting documentation
  - Added wake-up call
  - Added 30-second warm-up period
  - Added firmware version check
  - Added diagnostic warnings for zero readings
