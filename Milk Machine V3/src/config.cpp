// config.cpp

#include "config.h"

// WiFi credentials
const char * ssid = "WiFi";
const char * password = "Gliders1!";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 5);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Home Assistant details
const char * haUrl = "http://ha.richowen.me/api/states/sensor.milk_mixer_state";
const char * haToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJjZGM3YTYxYzMzYWQ0ZGE5ODI1MzUzZmVmYWFlYjUzNSIsImlhdCI6MTcyNzk4MzMyNywiZXhwIjoyMDQzMzQzMzI3fQ.v3gDsAgbXr5LfX-Y032ARICuzotNL9hyT-1zmqv_Iao";

// Pin definitions
const int mixerPin = 16;
const int waterPin = 17;
const int augerPin = 25;
const int agitatorPin = 26;
const int probe1Pin = 18;
const int probe2Pin = 19;
const int washStandbyPin = 23;
const int washDispensePin = 5;
const int liquidLevelPin = 12;  // Single pressure switch pin

// Timing constants
const unsigned long waitingDuration = 5000;
const unsigned long maxMixingDuration = 10000;
const unsigned long debounceDelay = 500;
const unsigned long LCD_UPDATE_INTERVAL = 1000;
const unsigned long DEBUG_UPDATE_INTERVAL = 5000;
unsigned long mixStartTime = 0;
bool mixTimerActive = false;
int lastStableReading = 0;
unsigned long lastStableTime = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 100;