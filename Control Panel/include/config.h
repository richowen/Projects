#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// SECRETS (WiFi & Home Assistant)
// ========================================
// Credentials are stored in secrets.h (not tracked by git)
// Copy secrets.h.example to secrets.h and fill in your details
#include "secrets.h"

// ========================================
// DEBUG CONFIGURATION
// ========================================
// Set to true for hardware testing (no WiFi/HTTP calls)
// Set to false for production use with Home Assistant
#define DEBUG_MODE false

// ========================================
// HOME ASSISTANT ENTITY IDs
// ========================================
// Update these to match your actual Home Assistant entity IDs

// AC control
#define ENTITY_AC_UNIT "switch.air_conditioner_switch"
#define ENTITY_AC_TEMP "number.air_conditioner_temp_set"

// AC Bypass - controls TWO entities
#define ENTITY_AC_BYPASS "input_boolean.ac_bypass"
#define ENTITY_AC_AUTOMATION "automation.ac_bypass_off"
#define ENTITY_PC_SHUTDOWN "automation.control_panel_pc_off"
#define ENTITY_PLEX "automation.control_panel_plex_on"
#define ENTITY_LIGHTS "automation.control_panel_lights"
#define ENTITY_IMMERSION "switch.immersion_switch"

// Light control
#define ENTITY_LIGHT_SCENE "scene.lights_evening"
#define ENTITY_LIGHTS_BRIGHTNESS "light.desk_lamp"

// Extra buttons (customize as needed)
#define ENTITY_EXTRA_1 "scene.lights_evening"
#define ENTITY_EXTRA_2 "switch.extra_2"
#define ENTITY_EXTRA_3 "switch.extra_3"
#define ENTITY_EXTRA_4 "switch.extra_4"
#define ENTITY_EXTRA_5 "switch.extra_5"

// ========================================
// PIN CONFIGURATION
// ========================================

// MAX7219 Display (Hardware SPI)
// NOTE: This project uses ESP32 hardware SPI pins:
//   - MOSI (Data): GPIO 23 (defined below but uses hardware SPI)
//   - SCK (Clock): GPIO 18 (defined below but uses hardware SPI)
//   - CS (Chip Select): GPIO 5 (user-configurable)
// If your wiring differs, you must use software SPI by modifying
// the MD_Parola constructor in main.cpp to include all pin parameters.
#define DISPLAY_CS_PIN 5
#define DISPLAY_CLK_PIN 18   // Hardware SPI SCK - for documentation only
#define DISPLAY_DATA_PIN 23  // Hardware SPI MOSI - for documentation only
#define MAX_DEVICES 1  // Number of 8x8 matrices chained together

// Potentiometer (Analog Input - ADC1 channels only)
#define POT_AC_TEMP_PIN 34

// Buttons and Switches (Digital Inputs with internal pullup)
#define BTN_AC_POWER_PIN 12
#define BTN_AC_BYPASS_PIN 13
#define BTN_PC_OFF_PIN 14
#define BTN_LIGHTS_PIN 15
#define BTN_IMMERSION_PIN 16
#define BTN_EXTRA_1_PIN 17
#define BTN_EXTRA_2_PIN 19
#define BTN_EXTRA_3_PIN 21
#define BTN_EXTRA_4_PIN 22
#define BTN_EXTRA_5_PIN 25

// Status LED
#define STATUS_LED_PIN 2

// ========================================
// TEMPERATURE SETTINGS
// ========================================
#define TEMP_MIN 18  // Minimum temperature for AC control (°C)
#define TEMP_MAX 31  // Maximum temperature for AC control (°C)

// ========================================
// BRIGHTNESS SETTINGS
// ========================================
#define BRIGHTNESS_MIN 0    // Minimum brightness percentage
#define BRIGHTNESS_MAX 100  // Maximum brightness percentage

// ========================================
// TIMING CONFIGURATION
// ========================================
#define DEBOUNCE_DELAY 50          // Button debounce time (ms)
#define DISPLAY_INTENSITY 3        // Display brightness (0-15)

// ========================================
// DISPLAY UX CONFIGURATION
// ========================================
// Display behavior
#define DISPLAY_IDLE_TIMEOUT 30000        // MS until screensaver (30 seconds)
#define DISPLAY_ACTION_DURATION 500       // MS to show action icon
#define DISPLAY_SUCCESS_DURATION 1000     // MS to show success
#define DISPLAY_ERROR_DURATION 1500       // MS to show error

// Animation settings
#define ENABLE_IDLE_ANIMATIONS false      // Screensaver animations (disabled per user preference)
#define IDLE_ANIMATION_DURATION 10000     // MS per idle animation pattern (10 seconds)
#define ANIMATION_FRAME_DELAY 100         // MS per animation frame
#define ENABLE_BOOT_ANIMATION true        // Cool startup sequence

// Visual preferences
#define SHOW_STATUS_INDICATORS false      // WiFi/HA status in corners (not yet implemented)
#define ENABLE_BUTTON_FLASH false         // Quick flash on button press (disabled - icons provide feedback)

#endif