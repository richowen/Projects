// In config.h:
#ifndef CONFIG_H
#define CONFIG_H
#define FILTER_SAMPLES 10

#include <Arduino.h>

// WiFi credentials
extern const char* ssid;
extern const char* password;

// Static IP configuration
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;

// Home Assistant details
extern const char* haUrl;
extern const char* haToken;

// Pin definitions
extern const int mixerPin;
extern const int waterPin;
extern const int augerPin;
extern const int agitatorPin;
extern const int probe1Pin;
extern const int probe2Pin;
extern const int washStandbyPin;
extern const int washDispensePin;
extern const int resetSwitchPin;
extern const int liquidLevelPin;

// Timing constants
extern const unsigned long waitingDuration;
extern const unsigned long maxMixingDuration;
extern const unsigned long debounceDelay;
extern const unsigned long LCD_UPDATE_INTERVAL;
extern const unsigned long DEBUG_UPDATE_INTERVAL;
extern const unsigned long UPDATE_INTERVAL;
extern unsigned long lastUpdate;

// Runtime variables
extern unsigned long lastStableTime;
extern int lastStableReading;
extern unsigned long lastDebugUpdate;

// Mix timer tracking
extern unsigned long mixStartTime;
extern bool mixTimerActive;

// Updated State enum
enum State {
    IDLE,
    MIXING,
    WAITING_POST_MIX,
    ERROR,
    WASH_STANDBY,
    WASH_DISPENSE
};

#endif // CONFIG_H