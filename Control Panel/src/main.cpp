#include <Arduino.h>
#include "esp_task_wdt.h"
#include "nvs_flash.h"

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Core system components
#include "core/config_manager.h"
#include "core/logger.h"
#include "network/wifi_manager.h"
#include "network/ha_client.h"
#include "io/input_manager.h"
#include "io/sensor_manager.h"
#include "display/display_manager.h"
#include "network/web_server.h"
#include "control_panel.h"
#include "config.h"


// ========================================
// HARDWARE SETUP
// ========================================

// Custom font for 8x8 display
MD_MAX72XX::fontType_t mediumFont[] PROGMEM = {
  1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0,
  1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0,
  1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0,
  1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0,
  1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0, 1,0,
  4,0x7E,0x81,0x81,0x7E,  // 0
  4,0x00,0xFF,0x00,0x00,  // 1
  4,0xF1,0x89,0x89,0x8E,  // 2
  4,0x81,0x89,0x89,0x76,  // 3
  4,0x0F,0x08,0xFF,0x08,  // 4
  4,0x8F,0x89,0x89,0x71,  // 5
  4,0x7E,0x89,0x89,0x71,  // 6
  4,0x01,0x01,0xFF,0x01,  // 7
  4,0x76,0x89,0x89,0x76,  // 8
  4,0x8E,0x89,0x89,0x7E   // 9
};

// MAX7219 Display
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, DISPLAY_CS_PIN, MAX_DEVICES);

// ========================================
// GLOBAL SYSTEM COMPONENTS
// ========================================

// Configuration and logging (foundational)
ConfigManager configManager;
Logger logger(Logger::INFO);

// Core managers
WiFiManager wifiManager(&configManager, &logger);
HAClient haClient(&configManager, &wifiManager, &logger);
InputManager inputManager(&configManager, &logger);
SensorManager sensorManager(&configManager, &logger);

// Display manager (uses existing implementation)
DisplayManager displayManager;

// Main control panel orchestrator
ControlPanel controlPanel(&configManager, &logger, &wifiManager, &haClient,
                         &inputManager, &sensorManager, &displayManager);

// Web server for live control panel configuration UI
WebServerManager webServerManager(&configManager, &inputManager, &wifiManager, &logger);


// ========================================
// BUTTON STATE TRACKING
// ========================================

// Input type: momentary button vs toggle switch
enum InputType {
  MOMENTARY_BUTTON,  // Press = send command once
  TOGGLE_SWITCH      // Position = send on/off based on state
};

struct ButtonState {
  int pin;
  bool lastState;
  bool currentState;
  unsigned long lastDebounceTime;
  const char* entityId;
  const char* service;
  InputType type;
};

// Button names for debug output
const char* buttonNames[] = {
  "AC Power", "AC Bypass", "PC Shutdown", "Lights", "Immersion",
  "Extra 1", "Plex On", "Extra 3", "Extra 4", "Extra 5"
};

// Button/switch configuration
// Format: {pin, lastState, currentState, debounceTime, entityId, service, type}
// For TOGGLE_SWITCH: service is used as domain (switch/light/etc), actual service is turn_on/turn_off
// For automation entities: use "trigger" service
ButtonState buttons[] = {
  {BTN_AC_POWER_PIN, HIGH, HIGH, 0, ENTITY_AC_UNIT, "toggle", MOMENTARY_BUTTON},
  {BTN_AC_BYPASS_PIN, HIGH, HIGH, 0, ENTITY_AC_BYPASS, "input_boolean", TOGGLE_SWITCH},
  {BTN_PC_OFF_PIN, HIGH, HIGH, 0, ENTITY_PC_SHUTDOWN, "trigger", MOMENTARY_BUTTON},
  {BTN_LIGHTS_PIN, HIGH, HIGH, 0, ENTITY_LIGHTS, "trigger", MOMENTARY_BUTTON},
  {BTN_IMMERSION_PIN, HIGH, HIGH, 0, ENTITY_IMMERSION, "switch", TOGGLE_SWITCH},
  {BTN_EXTRA_1_PIN, HIGH, HIGH, 0, ENTITY_EXTRA_1, "toggle", MOMENTARY_BUTTON},
  {BTN_EXTRA_2_PIN, HIGH, HIGH, 0, ENTITY_PLEX, "trigger", MOMENTARY_BUTTON},
  {BTN_EXTRA_3_PIN, HIGH, HIGH, 0, ENTITY_EXTRA_3, "toggle", MOMENTARY_BUTTON},
  {BTN_EXTRA_4_PIN, HIGH, HIGH, 0, ENTITY_EXTRA_4, "toggle", MOMENTARY_BUTTON},
  {BTN_EXTRA_5_PIN, HIGH, HIGH, 0, ENTITY_EXTRA_5, "toggle", MOMENTARY_BUTTON}
};

const int NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// Compile-time safety check
static_assert(sizeof(buttonNames)/sizeof(buttonNames[0]) == sizeof(buttons)/sizeof(buttons[0]),
              "buttonNames and buttons arrays must have same size");

// ========================================
// POTENTIOMETER STATE
// ========================================

int lastPotValue = -1;
unsigned long lastPotSendTime = 0;
int currentSetpoint = 20; // Current temperature setpoint from potentiometer

// Potentiometer debouncing - average multiple readings
#define POT_SAMPLES 10
int potReadings[POT_SAMPLES];
int potReadIndex = 0;
int potTotal = 0;
int potStableValue = -1;
unsigned long potStableTime = 0;
#define POT_STABLE_DURATION 500  // Value must be stable for 500ms before sending

// ========================================
// CONSTANTS
// ========================================

#define ADC_MAX_VALUE 4095           // ESP32 12-bit ADC maximum value
#define WIFI_CONNECT_ATTEMPTS 30     // Maximum WiFi connection attempts
#define HTTP_TIMEOUT_MS 5000         // HTTP request timeout (5 seconds)

// ========================================
// HOLD-TO-ACTIVATE SAFETY PERIODS
// ========================================

unsigned long pcShutdownHoldStart = 0;
bool pcShutdownInProgress = false;
#define PC_SHUTDOWN_HOLD_TIME 3000  // 3 seconds hold required

unsigned long plexOnHoldStart = 0;
bool plexOnInProgress = false;
#define PLEX_ON_HOLD_TIME 3000  // 3 seconds hold required

// ========================================
// WIFI RECONNECTION THROTTLING
// ========================================

unsigned long lastWiFiAttempt = 0;
#define WIFI_RETRY_INTERVAL 30000  // 30 seconds between reconnection attempts

// ========================================
// DISPLAY MANAGEMENT
// ========================================

unsigned long displayTimeout = 0;  // When to clear display
bool displayActive = false;        // Is display currently showing something
#define DISPLAY_TIMEOUT_MS 5000    // Clear display after 5 seconds
#define DISPLAY_HEIGHT 8           // LED matrix height in pixels

// ========================================
// FUNCTION DECLARATIONS
// ========================================

void connectWiFi();
void sendHomeAssistantCommand(const char* entityId, const char* service, JsonDocument* data = nullptr);
void handleButtons();
void handlePotentiometer();
void displayMessage(const char* msg);  // Legacy - kept for compatibility
void updateShutdownProgress();
void clearProgressBar();  // Legacy - kept for compatibility
void rotateDisplayCCW();  // Legacy - kept for compatibility

// ========================================
// SETUP
// ========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize NVS flash before anything reads persisted config from it.
  // (ConfigManager is a global object constructed before setup() runs, so
  // its constructor can't reliably use NVS yet - configManager.load() below
  // re-loads the saved input config now that NVS is ready.)
  esp_err_t nvsErr = nvs_flash_init();
  if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  configManager.load();

  Serial.println("\n\n");

  Serial.println("====================================");
  Serial.println("  HOME CONTROL PANEL - REFACTORED");
  Serial.println("====================================");
  Serial.println();

  // Initialize Status LED
  Serial.print("[INIT] Status LED (GPIO ");
  Serial.print(configManager.getPin("status_led"));
  Serial.print(")... ");
  pinMode(configManager.getPin("status_led"), OUTPUT);
  digitalWrite(configManager.getPin("status_led"), LOW);
  Serial.println("OK");

  // Initialize MAX7219 Display Hardware
  Serial.print("[INIT] MAX7219 Display... ");
  myDisplay.begin();
  myDisplay.setIntensity(DISPLAY_INTENSITY);
  myDisplay.setFont(mediumFont);
  myDisplay.setCharSpacing(0);
  myDisplay.displayClear();
  Serial.println("OK");

  // Initialize Display Manager
  Serial.print("[INIT] Display Manager... ");
  displayManager.init(&myDisplay);
  Serial.println("OK");

  // Initialize system components through control panel
  Serial.println("[INIT] Initializing control panel...");
  if (!controlPanel.begin()) {
    Serial.println("[ERROR] Control panel initialization failed!");
    while (true) {
      delay(1000);
    }
  }

  // Start web server for live configuration UI (requires WiFi to be connected)
  Serial.print("[INIT] Web Server... ");
  if (webServerManager.begin()) {
    Serial.println("OK");
    Serial.print("[INFO] Access the control panel UI at: http://");
    Serial.println(wifiManager.getIPAddress());
    Serial.println("[INFO] Or via: http://controlpanel.local");
  } else {
    Serial.println("FAILED");
  }

  Serial.println("\n====================================");
  Serial.println("         SETUP COMPLETE!");
  Serial.println("====================================");
  Serial.println("\nWaiting for input...\n");


  digitalWrite(configManager.getPin("status_led"), HIGH);

  // Enable watchdog timer for 30 seconds
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();

  // Update control panel (handles all system updates)
  controlPanel.update();

  // Slower loop in debug mode for readable serial output
  delay(configManager.isDebugMode() ? 50 : 10);
}

// All functionality moved to modular components