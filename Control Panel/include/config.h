#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// DEBUG CONFIGURATION
// ========================================
// Set to true for hardware testing (no WiFi/HTTP calls)
// Set to false for production use with Home Assistant
#define DEBUG_MODE false

// ========================================
// WIFI CONFIGURATION
// ========================================
// Replace with your WiFi credentials
#define WIFI_SSID "WiFi"
#define WIFI_PASSWORD "Gliders1!"

// ========================================
// HOME ASSISTANT CONFIGURATION
// ========================================
// Replace with your Home Assistant details
#define HA_URL "http://192.168.1.3:8123"  // Your Home Assistant URL
#define HA_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiIyNWIzYmRhZDk3MmI0NGQ3Yjc0NGU2MGQ3OGM5NzM5MSIsImlhdCI6MTc2MDk3MTgyNSwiZXhwIjoyMDc2MzMxODI1fQ.YwEZTq90yW8iXfJiS3pKW9Cgkid0Ti24Ct39E_0Qe8o"  // Generate in Home Assistant Profile

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
#define ENTITY_LIGHTS "light.lights"
#define ENTITY_IMMERSION "switch.immersion_switch"

// Extra buttons (customize as needed)
#define ENTITY_EXTRA_1 "switch.extra_1"
#define ENTITY_EXTRA_2 "switch.extra_2"
#define ENTITY_EXTRA_3 "switch.extra_3"
#define ENTITY_EXTRA_4 "switch.extra_4"
#define ENTITY_EXTRA_5 "switch.extra_5"

// ========================================
// PIN CONFIGURATION
// ========================================

// MAX7219 Display (SPI)
#define DISPLAY_CS_PIN 5
#define DISPLAY_CLK_PIN 18
#define DISPLAY_DATA_PIN 23
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
// TIMING CONFIGURATION
// ========================================
#define DEBOUNCE_DELAY 50          // Button debounce time (ms)
#define POT_SEND_INTERVAL 1000     // Min time between pot updates (ms)
#define POT_THRESHOLD 1            // Min temperature change to trigger update (°C)
#define TEMP_UPDATE_INTERVAL 5000  // Temperature display update interval (ms)
#define DISPLAY_INTENSITY 3        // Display brightness (0-15)

#endif