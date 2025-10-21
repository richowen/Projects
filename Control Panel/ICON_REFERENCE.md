# 8x8 LED Icon Reference Guide

## How to Read Icon Diagrams

- `■` = LED ON (lit pixel)
- `□` = LED OFF (dark pixel)
- Each icon is 8x8 pixels
- Designed for single 8x8 LED matrix display

## Action Icons

### AC / Air Conditioner
```
  □ □ ■ ■ ■ ■ □ □
  □ ■ □ □ □ □ ■ □
  □ ■ □ □ □ □ ■ □
  ■ □ ■ □ ■ □ □ ■
  ■ □ □ ■ □ ■ □ ■
  □ ■ □ □ □ □ ■ □
  □ ■ □ □ □ □ ■ □
  □ □ ■ ■ ■ ■ □ □
```
**Binary (for code):**
```cpp
const uint8_t ICON_AC[8] PROGMEM = {
    0b00111100,
    0b01000010,
    0b01000010,
    0b10010101,
    0b10100101,
    0b01000010,
    0b01000010,
    0b00111100
};
```

### Light Bulb
```
  □ □ □ ■ ■ □ □ □
  □ □ ■ □ □ ■ □ □
  □ □ ■ □ □ ■ □ □
  □ □ ■ ■ ■ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ ■ ■ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_LIGHT[8] PROGMEM = {
    0b00011000,
    0b00100100,
    0b00100100,
    0b00111100,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00011000
};
```

### Power Symbol
```
  □ □ □ ■ ■ □ □ □
  □ □ □ ■ ■ □ □ □
  □ ■ ■ ■ ■ ■ ■ □
  ■ □ □ □ □ □ □ ■
  ■ □ □ □ □ □ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ ■ ■ ■ □ □
  □ □ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_POWER[8] PROGMEM = {
    0b00011000,
    0b00011000,
    0b01111110,
    0b10000001,
    0b10000001,
    0b01000010,
    0b00111100,
    0b00000000
};
```

### Shutdown / Power Off
```
  □ □ □ ■ ■ □ □ □
  □ □ ■ ■ ■ ■ □ □
  □ ■ ■ □ □ ■ ■ □
  ■ ■ □ □ □ □ ■ ■
  ■ ■ □ □ □ □ ■ ■
  □ ■ ■ □ □ ■ ■ □
  □ □ ■ ■ ■ ■ □ □
  □ □ □ ■ ■ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_SHUTDOWN[8] PROGMEM = {
    0b00011000,
    0b00111100,
    0b01100110,
    0b11000011,
    0b11000011,
    0b01100110,
    0b00111100,
    0b00011000
};
```

### Computer / PC
```
  ■ ■ ■ ■ ■ ■ ■ ■
  ■ □ □ □ □ □ □ ■
  ■ □ ■ □ □ ■ □ ■
  ■ □ □ □ □ □ □ ■
  ■ ■ ■ ■ ■ ■ ■ ■
  □ □ □ ■ ■ □ □ □
  □ ■ ■ ■ ■ ■ ■ □
  ■ ■ ■ ■ ■ ■ ■ ■
```
**Binary:**
```cpp
const uint8_t ICON_PC[8] PROGMEM = {
    0b11111111,
    0b10000001,
    0b10100101,
    0b10000001,
    0b11111111,
    0b00011000,
    0b01111110,
    0b11111111
};
```

### Immersion Heater / Water
```
  □ ■ □ ■ □ ■ □ ■
  ■ □ ■ □ ■ □ ■ □
  □ ■ □ ■ □ ■ □ ■
  ■ □ ■ □ ■ □ ■ □
  □ ■ □ ■ □ ■ □ ■
  ■ □ ■ □ ■ □ ■ □
  □ ■ ■ ■ ■ ■ ■ □
  □ □ ■ ■ ■ ■ □ □
```
**Binary:**
```cpp
const uint8_t ICON_IMMERSION[8] PROGMEM = {
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01010101,
    0b10101010,
    0b01111110,
    0b00111100
};
```

### Play / Plex
```
  □ □ □ □ □ □ □ □
  □ ■ □ □ □ □ □ □
  □ ■ ■ □ □ □ □ □
  □ ■ ■ ■ □ □ □ □
  □ ■ ■ ■ ■ □ □ □
  □ ■ ■ ■ □ □ □ □
  □ ■ ■ □ □ □ □ □
  □ ■ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_PLAY[8] PROGMEM = {
    0b00000000,
    0b01000000,
    0b01100000,
    0b01110000,
    0b01111000,
    0b01110000,
    0b01100000,
    0b01000000
};
```

## Feedback Icons

### Checkmark (Success)
```
  □ □ □ □ □ □ □ ■
  □ □ □ □ □ □ ■ □
  □ □ □ □ □ ■ □ □
  □ □ □ □ ■ □ □ □
  ■ □ □ ■ □ □ □ □
  □ ■ ■ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_CHECK[8] PROGMEM = {
    0b00000001,
    0b00000010,
    0b00000100,
    0b00001000,
    0b10010000,
    0b01100000,
    0b00000000,
    0b00000000
};
```

### X Mark (Error)
```
  ■ □ □ □ □ □ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ □ □ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ ■ □ □ ■ □ □
  □ ■ □ □ □ □ ■ □
  ■ □ □ □ □ □ □ ■
  □ □ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_X[8] PROGMEM = {
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00100100,
    0b01000010,
    0b10000001,
    0b00000000
};
```

### Smiley Face (Happy)
```
  □ □ ■ ■ ■ ■ □ □
  □ ■ □ □ □ □ ■ □
  ■ □ ■ □ □ ■ □ ■
  ■ □ □ □ □ □ □ ■
  ■ □ ■ □ □ ■ □ ■
  ■ □ □ ■ ■ □ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ ■ ■ ■ □ □
```
**Binary:**
```cpp
const uint8_t ICON_SMILE[8] PROGMEM = {
    0b00111100,
    0b01000010,
    0b10100101,
    0b10000001,
    0b10100101,
    0b10011001,
    0b01000010,
    0b00111100
};
```

### Sad Face
```
  □ □ ■ ■ ■ ■ □ □
  □ ■ □ □ □ □ ■ □
  ■ □ ■ □ □ ■ □ ■
  ■ □ □ □ □ □ □ ■
  ■ □ □ ■ ■ □ □ ■
  ■ □ ■ □ □ ■ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ ■ ■ ■ □ □
```
**Binary:**
```cpp
const uint8_t ICON_SAD[8] PROGMEM = {
    0b00111100,
    0b01000010,
    0b10100101,
    0b10000001,
    0b10011001,
    0b10100101,
    0b01000010,
    0b00111100
};
```

### Heart
```
  □ ■ ■ □ □ ■ ■ □
  ■ □ □ ■ ■ □ □ ■
  ■ □ □ □ □ □ □ ■
  ■ □ □ □ □ □ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ □ □ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_HEART[8] PROGMEM = {
    0b01100110,
    0b10011001,
    0b10000001,
    0b10000001,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00000000
};
```

### Hourglass / Waiting
```
  ■ ■ ■ ■ ■ ■ ■ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ □ □ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ ■ ■ □ □ □
  □ □ ■ ■ ■ ■ □ □
  □ ■ ■ ■ ■ ■ ■ □
  ■ ■ ■ ■ ■ ■ ■ ■
```
**Binary:**
```cpp
const uint8_t ICON_HOURGLASS[8] PROGMEM = {
    0b11111111,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111
};
```

## Status Icons

### WiFi - No Signal
```
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ ■ □ □ □
  □ □ □ □ □ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_WIFI_0[8] PROGMEM = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00001000,
    0b00000000
};
```

### WiFi - Weak (1 Bar)
```
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ ■ □ □ □
  □ □ □ □ ■ ■ □ □
  □ □ □ □ ■ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_WIFI_1[8] PROGMEM = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00001000,
    0b00001100,
    0b00001000
};
```

### WiFi - Medium (2 Bars)
```
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ ■ □ □ □
  □ □ □ ■ ■ ■ □ □
  □ □ □ □ ■ □ □ □
  □ □ □ □ ■ ■ □ □
  □ □ □ □ ■ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_WIFI_2[8] PROGMEM = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00001000,
    0b00011100,
    0b00001000,
    0b00001100,
    0b00001000
};
```

### WiFi - Strong (3 Bars)
```
  □ □ □ □ ■ □ □ □
  □ □ □ ■ ■ ■ □ □
  □ □ ■ ■ ■ ■ ■ □
  □ □ □ □ ■ □ □ □
  □ □ □ ■ ■ ■ □ □
  □ □ □ □ ■ □ □ □
  □ □ □ □ ■ ■ □ □
  □ □ □ □ ■ □ □ □
```
**Binary:**
```cpp
const uint8_t ICON_WIFI_3[8] PROGMEM = {
    0b00001000,
    0b00011100,
    0b00111110,
    0b00001000,
    0b00011100,
    0b00001000,
    0b00001100,
    0b00001000
};
```

## Animation Frames

### Spinner (8 frames - rotating line)

**Frame 0:**
```
  □ □ □ ■ ■ □ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
```

**Frame 1:**
```
  □ □ □ □ □ ■ ■ □
  □ □ □ □ □ ■ ■ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
```

**Frame 2:**
```
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ ■ ■ □
  □ □ □ □ □ ■ ■ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
```

... continues rotating clockwise through 8 positions

### Heartbeat (4 frames)

**Frame 0: Small**
```
  □ □ □ □ □ □ □ □
  □ □ ■ □ □ ■ □ □
  □ ■ □ ■ ■ □ ■ □
  □ ■ □ □ □ □ ■ □
  □ □ ■ □ □ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ □ □ □ □ □
  □ □ □ □ □ □ □ □
```

**Frame 1: Large**
```
  □ ■ ■ □ □ ■ ■ □
  ■ □ □ ■ ■ □ □ ■
  ■ □ □ □ □ □ □ ■
  ■ □ □ □ □ □ □ ■
  □ ■ □ □ □ □ ■ □
  □ □ ■ □ □ ■ □ □
  □ □ □ ■ ■ □ □ □
  □ □ □ □ □ □ □ □
```

**Frame 2: Small (same as Frame 0)**

**Frame 3: Pause (blank for 200ms)**

## Icon Design Guidelines

1. **Keep it simple**: 8x8 is very limited, focus on recognizable shapes
2. **Use contrast**: Make sure icons are clearly visible
3. **Center important elements**: Eye naturally goes to center
4. **Test at distance**: Icons should be recognizable from 1-2 meters
5. **Avoid fine details**: Small details get lost at this resolution
6. **Use symmetry**: Symmetric icons are easier to recognize
7. **Reserve corners**: Leave corners for status indicators if needed

## Creating Custom Icons

### Method 1: Grid Paper
1. Draw 8x8 grid on paper
2. Sketch your icon
3. Fill in pixels
4. Convert to binary (each row = 8 bits)

### Method 2: Online Tools
- Use any pixel art editor set to 8x8
- Export as monochrome bitmap
- Convert to byte array

### Method 3: Code Generation
```python
# Python helper to convert ASCII art to binary
def icon_to_binary(icon_lines):
    for line in icon_lines:
        byte = 0
        for i, char in enumerate(line):
            if char == '■':
                byte |= (1 << (7-i))
        print(f"0b{byte:08b},")
```

## Memory Usage

- Each icon: 8 bytes
- 40 icons total: 320 bytes
- Stored in PROGMEM (Flash memory, not RAM)
- Runtime buffer: 8 bytes per active display

## Next Steps

See `DISPLAY_UX_ARCHITECTURE.md` for full implementation details.