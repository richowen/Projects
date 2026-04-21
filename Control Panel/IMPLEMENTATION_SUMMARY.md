# Light Scene and Potentiometer Refactor - Implementation Summary

## Overview
This implementation adds light scene control to Extra Button 1 and modifies the potentiometer to control brightness by default, switching to AC temperature control only when the AC bypass is active.

## Implemented Changes

### 1. Configuration ([`include/config.h`](include/config.h:32-38))
- Added `ENTITY_LIGHT_SCENE` for scene.lights_evening
- Added `ENTITY_LIGHTS_BRIGHTNESS` for light.lights
- Updated `ENTITY_EXTRA_1` to point to scene.lights_evening
- Added brightness range constants `BRIGHTNESS_MIN` (0) and `BRIGHTNESS_MAX` (100)

### 2. Interfaces ([`include/interfaces.h`](include/interfaces.h:186-209))
Updated `ISensorManager` interface with new methods:
- `setACBypassState(bool active)` - Switch potentiometer mode
- `isBrightnessMode()` - Check current mode
- `getBrightnessPercentage()` - Get brightness value
- `hasBrightnessChanged()` - Detect brightness changes

### 3. Sensor Manager Header ([`src/io/sensor_manager.h`](src/io/sensor_manager.h))
- Added `PotentiometerMode` enum (BRIGHTNESS/TEMPERATURE)
- Added mode tracking variables
- Added brightness-specific state variables
- Added helper methods for brightness calculations

### 4. Sensor Manager Implementation ([`src/io/sensor_manager.cpp`](src/io/sensor_manager.cpp))
- Constructor initializes in brightness mode by default
- `update()` method now routes to mode-specific update functions
- `setACBypassState()` switches modes and resets stability tracking
- `updateBrightnessMode()` calculates and tracks brightness changes
- `updateTemperatureMode()` handles temperature (existing logic)
- `adcToBrightness()` maps ADC (0-4095) to brightness (0-100%)

### 5. Control Panel Header ([`src/control_panel.h`](src/control_panel.h:57))
- Added `handleBrightnessChange()` method declaration

### 6. Control Panel Implementation ([`src/control_panel.cpp`](src/control_panel.cpp))
- `handleBrightnessChange()` sends brightness to Home Assistant with `brightness_pct` data
- `processSensors()` now checks mode and displays/handles accordingly:
  - Brightness mode: Shows "L##" format, sends to light entity
  - Temperature mode: Shows "##" format, sends to AC entity
- `handleToggleSwitch()` notifies sensor manager when AC bypass changes

### 7. Input Manager ([`src/io/input_manager.cpp`](src/io/input_manager.cpp:37))
- Changed Extra Button 1 service from "toggle" to "turn_on" (required for scenes)

### 8. Display Manager Header ([`src/display/display_manager.h`](src/display/display_manager.h:169))
- Added `showBrightness(int brightness)` method declaration

### 9. Display Manager Implementation ([`src/display/display_manager.cpp`](src/display/display_manager.cpp:462-476))
- Implemented `showBrightness()` to display "L##" format (L prefix for Light)

### 10. Config Manager ([`src/core/config_manager.cpp`](src/core/config_manager.cpp:100-101))
- Added mappings for "light_scene" and "lights_brightness" entity lookups

## How It Works

### System State Flow

1. **Startup**: System boots in brightness mode (default)
2. **Normal Operation**: Potentiometer controls light brightness (0-100%)
3. **AC Bypass ON**: User toggles AC bypass switch
   - Sensor manager switches to temperature mode
   - Potentiometer now controls AC temperature (18-31°C)
4. **AC Bypass OFF**: User toggles AC bypass switch back
   - Sensor manager switches back to brightness mode
   - Potentiometer returns to brightness control

### Display Indicators
- **Brightness Mode**: Shows "L##" (e.g., "L56" for 56% brightness)
- **Temperature Mode**: Shows "##" (e.g., "24" for 24°C)

### Extra Button 1
- Press triggers scene.lights_evening using turn_on service
- Displays action icon feedback

## Home Assistant Configuration Required

Users need to ensure these entities exist in their Home Assistant setup:

```yaml
# Scene entity
scene.lights_evening

# Light entity (or group)
light.lights

# Existing entities (unchanged)
input_boolean.ac_bypass
automation.ac_bypass_off
number.air_conditioner_temp_set
```

## API Calls to Home Assistant

### Brightness Control
```json
POST /api/services/light/turn_on
{
  "entity_id": "light.lights",
  "brightness_pct": 56
}
```

### Scene Activation
```json
POST /api/services/scene/turn_on
{
  "entity_id": "scene.lights_evening"
}
```

### Temperature Control (when AC bypass active)
```json
POST /api/services/number/set_value
{
  "entity_id": "number.air_conditioner_temp_set",
  "value": 24
}
```

## Key Technical Details

### ADC Mapping
- **Brightness**: ADC 0-4095 → 0-100%
- **Temperature**: ADC 0-4095 → 18-31°C (inverted for typical pot installation)

### Stability & Debouncing
- Uses existing 10-sample averaging
- 500ms stability duration before sending changes
- Stability tracking resets when switching modes

### Mode Switching
- Mode changes only when AC bypass switch changes state
- No automatic mode switching or timeouts
- Clear visual feedback via display prefix

## Testing Checklist

- [ ] Extra Button 1 activates scene.lights_evening
- [ ] Potentiometer controls brightness by default (shows "L##")
- [ ] AC bypass ON switches to temperature mode (shows "##")
- [ ] AC bypass OFF switches back to brightness mode
- [ ] Brightness changes send to light.lights with correct percentage
- [ ] Temperature changes send to AC entity with correct value
- [ ] Display updates smoothly without flicker
- [ ] No false triggers during mode switching

## Files Modified Summary

| File | Lines Changed | Type |
|------|---------------|------|
| include/config.h | +9 | Configuration |
| include/interfaces.h | +24 | Interface definitions |
| src/io/sensor_manager.h | +26 | Header |
| src/io/sensor_manager.cpp | +77 | Implementation |
| src/control_panel.h | +5 | Header |
| src/control_panel.cpp | +38 | Implementation |
| src/io/input_manager.cpp | +1 | Configuration |
| src/display/display_manager.h | +3 | Header |
| src/display/display_manager.cpp | +15 | Implementation |
| src/core/config_manager.cpp | +2 | Mappings |

**Total: 10 files modified, ~200 lines added/changed**

## Backward Compatibility

- Existing temperature control functionality preserved
- AC bypass behavior unchanged
- All other buttons/switches work as before
- System gracefully defaults to brightness mode if AC bypass state unknown

## Future Enhancements (Not Implemented)

- Multiple scene selection via button hold patterns
- Brightness presets via double-tap
- Persistent memory of last brightness/temperature values
- Visual mode indicator in idle state
- Smooth brightness transitions
