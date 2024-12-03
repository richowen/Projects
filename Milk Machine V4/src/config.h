// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Definitions
#define MIXER_PIN         16    // Mixer motor relay
#define WATER_PIN         17    // Water dispenser relay
#define AUGER_PIN         25    // Powder auger relay
#define AGITATOR_PIN      26    // Powder agitator relay
#define WASH_STANDBY_PIN  23    // Wash standby switch
#define WASH_DISPENSE_PIN 5     // Water solenoid activate switch in wash mode
#define LIQUID_LEVEL_PIN  12    // Liquid level pressure switch
#define AUGER_CURRENT_PIN 34    // Auger motor current sensor
#define MIXER_CURRENT_PIN 35    // Mixer motor current sensor

// Relay States (Active LOW relays)
#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

// Switch States (Connected to VCC when ON)
#define SWITCH_ON   HIGH
#define SWITCH_OFF  LOW

// Timing Configuration (all times in milliseconds)
#define POST_MIXING_DURATION    5000    // 5 seconds post-mix time
#define MIXING_TIMEOUT         60000    // 60 seconds mix timeout
#define MIX_INTERVAL         300000    // 5 minutes between idle mixes
#define IDLE_MIX_DURATION      5000    // 5 seconds idle mix duration
#define CURRENT_CHECK_INTERVAL  100     // Check current every 100ms
#define DEBOUNCE_DELAY         500     // Switch debounce time

// Motor Current Monitoring
#define EMPTY_HOPPER_THRESHOLD   0.3f   // 300mA - Auger running empty
#define MIXER_OVERLOAD_THRESHOLD 2.0f   // 2.0A - Mixer jam protection
#define EMPTY_HOPPER_COUNT        10    // Consecutive low readings for empty
#define OVERLOAD_COUNT             5    // Consecutive high readings for overload

// ACS712 Configuration
#define VCC             3.3f    // ADC reference voltage
#define MV_PER_AMP    100.0f   // 100mV/A for ACS712-20A
#define ADC_RESOLUTION 4096.0f  // 12-bit ADC

// WiFi Configuration
#define WIFI_SSID       "WiFi"
#define WIFI_PASSWORD   "Gliders1!"
#define STATIC_IP       IPAddress(192,168,1,5)
#define GATEWAY         IPAddress(192,168,1,1)
#define SUBNET          IPAddress(255,255,255,0)
#define HOSTNAME        "Milk_Machine"

// LCD Configuration
#define LCD_COLS        16
#define LCD_ROWS        2

// State Display Colors (RGB values)
#define COLOR_IDLE_R      0
#define COLOR_IDLE_G    255
#define COLOR_IDLE_B      0

#define COLOR_MIXING_R    0
#define COLOR_MIXING_G    0
#define COLOR_MIXING_B  255

#define COLOR_POST_MIX_R   0
#define COLOR_POST_MIX_G 255
#define COLOR_POST_MIX_B 255

#define COLOR_WASH_R    255
#define COLOR_WASH_G    165
#define COLOR_WASH_B      0

#define COLOR_ERROR_R   255
#define COLOR_ERROR_G     0
#define COLOR_ERROR_B     0

// Error Messages
#define ERROR_MSG_TIMEOUT  "Mixing Timeout"
#define ERROR_MSG_MOTOR    "Motor Current!"
#define ERROR_MSG_WATER    "Water Pressure!"
#define ERROR_MSG_HOPPER   "Hopper Empty!"
#define ERROR_MSG_UNKNOWN  "Unknown Error"

#endif // CONFIG_H
