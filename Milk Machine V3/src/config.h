// config.h

#ifndef CONFIG_H
#define CONFIG_H

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
extern const int sensorPin;
extern const int ledPin;

// Timing constants
extern const unsigned long waitingDuration;
extern const unsigned long maxMixingDuration;
extern const unsigned long debounceDelay;

// State enum
enum State {
  IDLE,
  WAITING_PRE_MIX,
  MIXING,
  WAITING_POST_MIX,
  ERROR
};

#endif // CONFIG_H