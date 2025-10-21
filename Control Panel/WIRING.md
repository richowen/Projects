# ESP32 Control Panel - Wiring Guide

## Wiring Diagram Overview

This guide provides detailed wiring instructions for connecting all components to the ESP32.

## Component List

- 1x ESP32 Development Board
- 1x MAX7219 Dot Matrix Display (4x 8x8 modules)
- 1x 10kΩ Potentiometer
- 10x Pushbuttons or Toggle Switches
- 1x LED (optional - can use built-in)
- Jumper wires
- Breadboard (optional)

## Power Considerations

**Important**: The ESP32 operates at 3.3V logic, but most MAX7219 modules work with 5V.

- ESP32 VIN: 5V input (when using USB or external 5V supply)
- ESP32 3.3V: 3.3V output (max ~500mA)
- MAX7219: Can be powered from 5V (VIN) for brighter LEDs
- Buttons: Use 3.3V with internal pull-ups

## MAX7219 Display Wiring

```
ESP32          MAX7219 Module
----------------------------------
GPIO 5    ---> CS (Chip Select)
GPIO 18   ---> CLK (Clock)
GPIO 23   ---> DIN (Data In)
GND       ---> GND
VIN (5V)  ---> VCC (for brighter display)
  or
3.3V      ---> VCC (dimmer, but safer)
```

**Note**: If using multiple MAX7219 modules in chain:
- Connect DOUT of first module to DIN of second module
- Only first module connects to ESP32
- All modules share VCC, GND, CLK, and CS

## Potentiometer Wiring (Temperature Control)

```
ESP32          Potentiometer
----------------------------------
GPIO 34   ---> Wiper (middle pin)
3.3V      ---> One outer pin
GND       ---> Other outer pin
```

**Important**: 
- Use ADC1 channels only (GPIO 32-39)
- GPIO 34 is input-only, perfect for analog reading
- Don't use ADC2 channels (GPIO 0, 2, 4, 12-15, 25-27) as they conflict with WiFi

## Button Wiring (with Internal Pull-up)

Each button uses this configuration:

```
ESP32 GPIO ---[Button]--- GND
```

The ESP32's internal pull-up resistor keeps the pin HIGH when button is not pressed.
When pressed, the button connects the pin to GND (LOW).

### Button Pin Assignments

```
Button Function    GPIO    Wiring
------------------------------------------
AC Power           12      Button to GND
AC Bypass          13      Button to GND
PC Off             14      Button to GND
Lights             15      Button to GND
Immersion          16      Button to GND
Extra 1            17      Button to GND
Extra 2            19      Button to GND
Extra 3            21      Button to GND
Extra 4            22      Button to GND
Extra 5            25      Button to GND
```

**Alternative (with External Pull-up)**:
If you prefer external pull-ups:
```
3.3V ---[10kΩ]--- GPIO
                   |
              [Button]
                   |
                  GND
```
Then modify code to remove `INPUT_PULLUP` and use `INPUT`.

## Status LED Wiring

### Using Built-in LED (Recommended)
```
GPIO 2 is connected to built-in LED on most ESP32 boards
No wiring needed!
```

### Using External LED
```
ESP32          LED Circuit
----------------------------------
GPIO 2    ---> [330Ω] ---> LED+ (Anode)
                           LED- (Cathode) ---> GND
```

## Complete Pin Summary

```
GPIO    Function                Type        Connection
----------------------------------------------------------------
2       Status LED              Output      Built-in LED or external
5       MAX7219 CS              Output      Display CS pin
12      Button: AC Power        Input       Button to GND
13      Button: AC Bypass       Input       Button to GND
14      Button: PC Off          Input       Button to GND
15      Button: Lights          Input       Button to GND
16      Button: Immersion       Input       Button to GND
17      Button: Extra 1         Input       Button to GND
18      MAX7219 CLK             Output      Display CLK pin
19      Button: Extra 2         Input       Button to GND
21      Button: Extra 3         Input       Button to GND
22      Button: Extra 4         Input       Button to GND
23      MAX7219 DIN             Output      Display DIN pin
25      Button: Extra 5         Input       Button to GND
34      Potentiometer           Analog In   Pot wiper (center pin)
GND     Ground                  Power       All GND connections
3.3V    Power (3.3V)            Power       Potentiometer VCC
VIN     Power (5V)              Power       MAX7219 VCC (optional)
```

## Breadboard Layout Example

```
        ESP32
    +-----------+
    |           |
    | 3.3V  GND |----+
    |           |    |
    | GPIO2     |----+-----[LED]-----[330Ω]---+
    | GPIO5     |----+                        |
    | GPIO12    |----+--[Button]--+           |
    | GPIO13    |----+--[Button]--+           |
    | GPIO14    |----+--[Button]--+           |
    | GPIO15    |----+--[Button]--+           |
    | GPIO16    |----+--[Button]--+           |
    | GPIO17    |----+--[Button]--+           |
    | GPIO18    |----+                        |
    | GPIO19    |----+--[Button]--+           |
    | GPIO21    |----+--[Button]--+           |
    | GPIO22    |----+--[Button]--+           |
    | GPIO23    |----+                        |
    | GPIO25    |----+--[Button]--+           |
    | GPIO34    |----+                        |
    | VIN       |----+                        |
    +-----------+    |                        |
                     |                        |
                  [GND Rail]------------------+
                     
```

## Troubleshooting Wiring Issues

### Display Not Working
- Check VCC connection (should have stable 5V or 3.3V)
- Verify CS, CLK, DIN connections
- Try swapping CLK and DIN if display shows garbage
- Check GND connection

### Buttons Not Detected
- Verify GPIO pins match configuration
- Check button is wiring to GND, not VCC
- Test button continuity with multimeter
- Ensure no GPIO conflicts (e.g., strapping pins)

### Potentiometer Erratic
- Check wiper connection to GPIO 34
- Verify VCC is 3.3V (not 5V!)
- Ensure good GND connection
- Try different ADC pin if GPIO 34 doesn't work

### Power Issues
- Don't power MAX7219 display from ESP32 3.3V if using all LEDs
- Use external 5V supply for display if needed
- Ensure adequate USB power (500mA minimum)

## GPIO Limitations & Warnings

### Avoid These Pins During Boot
- GPIO 0: Boot mode (must be HIGH during boot)
- GPIO 2: Boot mode (must be LOW during boot, has internal LED)
- GPIO 12: Boot voltage selection
- GPIO 15: Boot mode

### Input Only Pins
- GPIO 34-39: Input only, no pull-up/down resistors

### ADC2 Pins (Don't Use with WiFi)
- GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27

### Safe Pins for General Use
- GPIO 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33

## Advanced: PCB Design Considerations

If you're designing a custom PCB:

1. **Pull-up Resistors**: Add 10kΩ pull-ups on all button pins
2. **Decoupling Capacitors**: 
   - 100nF near ESP32 VCC
   - 100µF on MAX7219 power supply
3. **Current Limiting**: 330Ω resistor for status LED
4. **ESD Protection**: Consider TVS diodes on button inputs
5. **Mounting**: Add mounting holes for enclosure
6. **Connectors**: Use JST or screw terminals for easy assembly

## 3D Printing Enclosure Tips

- Leave space for MAX7219 display to be visible
- Access to USB port for programming
- Button mounting holes
- Ventilation for ESP32
- Cable management channels

## Testing Procedure

1. **Power Test**: Connect only power, verify 3.3V and 5V rails
2. **Display Test**: Connect display, run basic test code
3. **Button Test**: Connect one button, test in serial monitor
4. **Potentiometer Test**: Connect pot, read analog values
5. **Full Integration**: Connect all components, test each function

## Safety Notes

⚠️ **Important Safety Warnings**:
- Never connect 5V directly to GPIO pins
- Don't exceed 12mA per GPIO pin
- Total GPIO current should not exceed 200mA
- Use proper wire gauge for power connections
- Double-check polarity before applying power
- Disconnect power when making wiring changes

## Component Alternatives

If you can't find exact components:

- **Display**: Any SPI MAX7219 display (even 7-segment)
- **Buttons**: Momentary or latching switches
- **Potentiometer**: 1kΩ - 100kΩ will work (10kΩ recommended)
- **ESP32**: Most ESP32 boards work (check pin compatibility)

## Next Steps

After wiring:
1. Double-check all connections
2. Review `include/config.h` for pin assignments
3. Upload code via USB
4. Monitor serial output for debugging
5. Test each button individually
6. Calibrate potentiometer range if needed