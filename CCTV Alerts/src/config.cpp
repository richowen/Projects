#include "config.h"

// Network credentials and settings
const char* WIFI_SSID = "IoT";
const char* WIFI_PASSWORD = "Gliders1!";
IPAddress STATIC_IP(192, 168, 1, 21);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress PRIMARY_DNS(192, 168, 1, 1);
IPAddress SECONDARY_DNS(8, 8, 4, 4);

// Server and pin settings
const int LED_PIN = 14;
const int SPEAKER_PIN = 13;

// System monitoring
const int WDT_TIMEOUT = 30;
RTC_DATA_ATTR int bootCount = 0;

// Timing settings
const unsigned long MIN_RECONNECT_INTERVAL = 5000;
const unsigned long MAX_RECONNECT_INTERVAL = 300000;

// LED settings
const int LED_CHANNEL = 0;
const int LED_FREQ = 800;
const int LED_RESOLUTION = 8;
const int BREATHE_PERIOD = 2000;
const unsigned long BREATHING_DURATION = 5000;

// Status variables
bool wifiConnected = false;
bool breathing = false;
bool fireAlarmActive = false;

// Timing variables
unsigned long startTime = 0;
unsigned long currentReconnectInterval = MIN_RECONNECT_INTERVAL;
unsigned long lastReconnectAttempt = 0;
unsigned long lastFlashTime = 0;
const int FLASH_DURATION = 500;
const int FIRE_ALARM_FLASH_DURATION = 100;
unsigned long breathingStartTime = 0;
unsigned long lastFireAlarmTone = 0;
const int FIRE_ALARM_TONE_INTERVAL = 500;

// WiFi settings
const int WIFI_RSSI_THRESHOLD = -75;
const int WIFI_SCAN_TIMEOUT = 10000;

// System metrics
unsigned long lastHeapCheck = 0;
const unsigned long HEAP_CHECK_INTERVAL = 60000;
size_t lastFreeHeap = 0;

// Task handles
TaskHandle_t wifiTaskHandle;
TaskHandle_t healthTaskHandle;

// Preferences
Preferences preferences;