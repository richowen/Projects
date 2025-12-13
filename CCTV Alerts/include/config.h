#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// Network credentials and settings
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern IPAddress STATIC_IP;
extern IPAddress GATEWAY;
extern IPAddress SUBNET;
extern IPAddress PRIMARY_DNS;
extern IPAddress SECONDARY_DNS;

// Server and pin settings
extern const int LED_PIN;
extern const int SPEAKER_PIN;

// System monitoring
extern const int WDT_TIMEOUT;
extern RTC_DATA_ATTR int bootCount;

// Timing settings
extern const unsigned long MIN_RECONNECT_INTERVAL;
extern const unsigned long MAX_RECONNECT_INTERVAL;

// LED settings
extern const int LED_CHANNEL;
extern const int LED_FREQ;
extern const int LED_RESOLUTION;
extern const int BREATHE_PERIOD;
extern const unsigned long BREATHING_DURATION;

// Status variables
extern bool wifiConnected;
extern bool breathing;
extern bool fireAlarmActive;

// Timing variables
extern unsigned long startTime;
extern unsigned long currentReconnectInterval;
extern unsigned long lastReconnectAttempt;
extern unsigned long lastFlashTime;
extern const int FLASH_DURATION;
extern const int FIRE_ALARM_FLASH_DURATION;
extern unsigned long breathingStartTime;
extern unsigned long lastFireAlarmTone;
extern const int FIRE_ALARM_TONE_INTERVAL;

// WiFi settings
extern const int WIFI_RSSI_THRESHOLD;
extern const int WIFI_SCAN_TIMEOUT;

// System metrics
extern unsigned long lastHeapCheck;
extern const unsigned long HEAP_CHECK_INTERVAL;
extern size_t lastFreeHeap;

// Task handles
extern TaskHandle_t wifiTaskHandle;
extern TaskHandle_t healthTaskHandle;

// Preferences
extern Preferences preferences;

#endif // CONFIG_H