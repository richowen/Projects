#ifndef DISPLAY_ICONS_H
#define DISPLAY_ICONS_H

#include <Arduino.h>

// ========================================
// 8x8 PIXEL ART ICON LIBRARY
// ========================================
// Each icon is 8 bytes (one byte per row)
// Bit 7 (MSB) = leftmost pixel, Bit 0 (LSB) = rightmost pixel
// After 90° CCW rotation, these display correctly on physical matrix

// Icon indices for easy reference
enum IconIndex {
    // Action Icons
    ICON_AC = 0,
    ICON_LIGHT,
    ICON_POWER,
    ICON_SHUTDOWN,
    ICON_PC,
    ICON_IMMERSION,
    ICON_PLAY,
    
    // Feedback Icons
    ICON_CHECK,
    ICON_X,
    ICON_SMILE,
    ICON_SAD,
    ICON_HEART,
    ICON_HOURGLASS,
    
    // Status Icons
    ICON_WIFI_0,
    ICON_WIFI_1,
    ICON_WIFI_2,
    ICON_WIFI_3,
    
    // Spinner Animation Frames (8 frames)
    ICON_SPINNER_0,
    ICON_SPINNER_1,
    ICON_SPINNER_2,
    ICON_SPINNER_3,
    ICON_SPINNER_4,
    ICON_SPINNER_5,
    ICON_SPINNER_6,
    ICON_SPINNER_7,
    
    // Total count
    ICON_COUNT
};

// ========================================
// ICON DATA (stored in PROGMEM to save RAM)
// ========================================

const uint8_t ICONS[][8] PROGMEM = {
    // AC / Air Conditioner
    {
        0b00111100,
        0b01000010,
        0b01000010,
        0b10010101,
        0b10100101,
        0b01000010,
        0b01000010,
        0b00111100
    },
    
    // Light Bulb
    {
        0b00011000,
        0b00100100,
        0b00100100,
        0b00111100,
        0b00011000,
        0b00011000,
        0b00000000,
        0b00011000
    },
    
    // Power Symbol
    {
        0b00011000,
        0b00011000,
        0b01111110,
        0b10000001,
        0b10000001,
        0b01000010,
        0b00111100,
        0b00000000
    },
    
    // Shutdown / Power Off
    {
        0b00011000,
        0b00111100,
        0b01100110,
        0b11000011,
        0b11000011,
        0b01100110,
        0b00111100,
        0b00011000
    },
    
    // Computer / PC
    {
        0b11111111,
        0b10000001,
        0b10100101,
        0b10000001,
        0b11111111,
        0b00011000,
        0b01111110,
        0b11111111
    },
    
    // Immersion Heater / Water
    {
        0b01010101,
        0b10101010,
        0b01010101,
        0b10101010,
        0b01010101,
        0b10101010,
        0b01111110,
        0b00111100
    },
    
    // Play / Plex
    {
        0b00000000,
        0b01000000,
        0b01100000,
        0b01110000,
        0b01111000,
        0b01110000,
        0b01100000,
        0b01000000
    },
    
    // Checkmark (Success)
    {
        0b00000001,
        0b00000010,
        0b00000100,
        0b00001000,
        0b10010000,
        0b01100000,
        0b00000000,
        0b00000000
    },
    
    // X Mark (Error)
    {
        0b10000001,
        0b01000010,
        0b00100100,
        0b00011000,
        0b00100100,
        0b01000010,
        0b10000001,
        0b00000000
    },
    
    // Smiley Face (Happy)
    {
        0b00111100,
        0b01000010,
        0b10100101,
        0b10000001,
        0b10100101,
        0b10011001,
        0b01000010,
        0b00111100
    },
    
    // Sad Face
    {
        0b00111100,
        0b01000010,
        0b10100101,
        0b10000001,
        0b10011001,
        0b10100101,
        0b01000010,
        0b00111100
    },
    
    // Heart
    {
        0b01100110,
        0b10011001,
        0b10000001,
        0b10000001,
        0b01000010,
        0b00100100,
        0b00011000,
        0b00000000
    },
    
    // Hourglass / Waiting
    {
        0b11111111,
        0b01000010,
        0b00100100,
        0b00011000,
        0b00011000,
        0b00111100,
        0b01111110,
        0b11111111
    },
    
    // WiFi - No Signal
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00001000,
        0b00000000
    },
    
    // WiFi - Weak (1 Bar)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00001000,
        0b00001100,
        0b00001000
    },
    
    // WiFi - Medium (2 Bars)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00001000,
        0b00011100,
        0b00001000,
        0b00001100,
        0b00001000
    },
    
    // WiFi - Strong (3 Bars)
    {
        0b00001000,
        0b00011100,
        0b00111110,
        0b00001000,
        0b00011100,
        0b00001000,
        0b00001100,
        0b00001000
    },
    
    // Spinner Frame 0 (Top)
    {
        0b00011000,
        0b00011000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    },
    
    // Spinner Frame 1 (Top-Right)
    {
        0b00000000,
        0b00000110,
        0b00000110,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    },
    
    // Spinner Frame 2 (Right)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000011,
        0b00000011,
        0b00000000,
        0b00000000,
        0b00000000
    },
    
    // Spinner Frame 3 (Bottom-Right)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000110,
        0b00000110,
        0b00000000
    },
    
    // Spinner Frame 4 (Bottom)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00011000,
        0b00011000
    },
    
    // Spinner Frame 5 (Bottom-Left)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b01100000,
        0b01100000,
        0b00000000
    },
    
    // Spinner Frame 6 (Left)
    {
        0b00000000,
        0b00000000,
        0b00000000,
        0b11000000,
        0b11000000,
        0b00000000,
        0b00000000,
        0b00000000
    },
    
    // Spinner Frame 7 (Top-Left)
    {
        0b00000000,
        0b01100000,
        0b01100000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000
    }
};

// Helper function to get icon data
inline const uint8_t* getIcon(uint8_t iconIndex) {
    if (iconIndex >= ICON_COUNT) return nullptr;
    return ICONS[iconIndex];
}

// Helper function to read icon byte from PROGMEM
inline uint8_t getIconByte(uint8_t iconIndex, uint8_t row) {
    if (iconIndex >= ICON_COUNT || row >= 8) return 0;
    return pgm_read_byte(&ICONS[iconIndex][row]);
}

#endif // DISPLAY_ICONS_H