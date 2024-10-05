// config.h

#ifndef CONFIG_H               // Include guard to prevent multiple inclusions of this header file
#define CONFIG_H

#define FILTER_SAMPLES 10      // Define the number of samples for filtering sensor data

#include <Arduino.h>           // Include Arduino core library for basic functions

// WiFi credentials
extern const char* ssid;       // External declaration for WiFi SSID
extern const char* password;   // External declaration for WiFi password

// Static IP configuration
extern IPAddress local_IP;     // External declaration for local static IP address
extern IPAddress gateway;      // External declaration for gateway IP address
extern IPAddress subnet;       // External declaration for subnet mask
extern IPAddress primaryDNS;   // External declaration for primary DNS server
extern IPAddress secondaryDNS; // External declaration for secondary DNS server

// Home Assistant details
extern const char* haUrl;      // External declaration for Home Assistant URL
extern const char* haToken;    // External declaration for Home Assistant authentication token

// Pin definitions
extern const int mixerPin;        // External declaration for mixer pin number
extern const int waterPin;        // External declaration for water control pin number
extern const int augerPin;        // External declaration for auger control pin number
extern const int agitatorPin;     // External declaration for agitator control pin number
extern const int sensorPin;       // External declaration for sensor pin number
extern const int ledPin;          // External declaration for LED pin number
extern const int washStandbyPin;  // External declaration for wash standby pin number
extern const int washDispensePin; // External declaration for wash dispense pin number
extern const int resetSwitchPin;  // External declaration for reset switch pin number

// Timing constants
extern const unsigned long waitingDuration;      // External declaration for duration to wait before mixing
extern const unsigned long maxMixingDuration;    // External declaration for maximum duration for mixing
extern const unsigned long debounceDelay;        // External declaration for debounce delay time
extern const unsigned long LCD_UPDATE_INTERVAL;  // External declaration for LCD update interval time

// Updated State enum
enum State {                                     // Enum to represent various states of the system
  IDLE,                                          // IDLE state
  WAITING_PRE_MIX,                               // Waiting before mixing
  MIXING,                                        // Mixing state
  WAITING_POST_MIX,                              // Waiting after mixing
  ERROR,                                         // Error state
  WASH_STANDBY,                                  // Wash standby state
  WASH_DISPENSE                                  // Wash dispense state
};

#endif // CONFIG_H          // End of include guard
