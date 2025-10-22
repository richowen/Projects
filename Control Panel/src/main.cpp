#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"
#include "display/display_manager.h"

// ========================================
// HARDWARE SETUP
// ========================================

// MAX7219 Display - Using MD_Parola for simple text display
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, DISPLAY_CS_PIN, MAX_DEVICES);

// Display Manager for enhanced UX
DisplayManager displayManager;

// ========================================
// CUSTOM TINY FONT FOR 8x8 DISPLAY
// ========================================

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
  
  Serial.println("\n\n");
  Serial.println("====================================");
  if (DEBUG_MODE) {
    Serial.println("  HOME CONTROL PANEL - DEBUG MODE");
  } else {
    Serial.println("  HOME CONTROL PANEL - PRODUCTION");
  }
  Serial.println("====================================");
  Serial.println();

  // Initialize Status LED
  Serial.print("[INIT] Status LED (GPIO ");
  Serial.print(STATUS_LED_PIN);
  Serial.print(")... ");
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  Serial.println("OK");

  // Initialize Display
  Serial.print("[INIT] MAX7219 Display... ");
  myDisplay.begin();
  myDisplay.setIntensity(DISPLAY_INTENSITY);
  myDisplay.setFont(mediumFont);  // Use custom 4x8 font
  myDisplay.setCharSpacing(0);  // No spacing between digits for 8x8 display
  myDisplay.displayClear();
  Serial.println("OK");
  
  // Initialize Display Manager
  Serial.print("[INIT] Display Manager... ");
  displayManager.init(&myDisplay);
  Serial.println("OK");
  
  #if ENABLE_BOOT_ANIMATION
    Serial.println("[DISPLAY] Starting boot sequence...");
    displayManager.showBootSequence();
    delay(1500);  // Let boot animation play
  #else
    displayMessage("00");
    delay(500);
  #endif
  
  // Initialize Buttons with internal pull-up resistors
  Serial.println("\n[INIT] Button/Switch Configuration:");
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastState = digitalRead(buttons[i].pin);
    buttons[i].currentState = buttons[i].lastState;
    
    const char* typeStr = (buttons[i].type == MOMENTARY_BUTTON) ? "Button" : "Toggle";
    
    // Use Serial.print to avoid buffer overflow with long entity IDs
    Serial.print("  [");
    Serial.print(i + 1);
    Serial.print("] ");
    Serial.print(buttonNames[i]);
    Serial.print(" (GPIO ");
    Serial.print(buttons[i].pin);
    Serial.print(") - ");
    Serial.print(typeStr);
    Serial.print(" - ");
    Serial.println(buttons[i].entityId);
  }

  // Initialize Potentiometer with averaging
  Serial.print("\n[INIT] Potentiometer (GPIO ");
  Serial.print(POT_AC_TEMP_PIN);
  Serial.print(")... ");
  pinMode(POT_AC_TEMP_PIN, INPUT);
  
  // Initialize averaging array
  for (int i = 0; i < POT_SAMPLES; i++) {
    potReadings[i] = analogRead(POT_AC_TEMP_PIN);
    potTotal += potReadings[i];
    delay(10);
  }
  int avgReading = potTotal / POT_SAMPLES;
  currentSetpoint = map(avgReading, 0, 4095, TEMP_MAX, TEMP_MIN);
  lastPotValue = currentSetpoint;
  potStableValue = currentSetpoint;
  
  Serial.print("OK (Initial: ");
  Serial.print(currentSetpoint);
  Serial.println("°C)");

  // WiFi Connection
  if (DEBUG_MODE) {
    Serial.println("\n[WiFi] SKIPPED - Debug Mode Enabled");
    Serial.println("       (All network calls will be simulated)");
    displayMessage("88");
    delay(1000);
  } else {
    connectWiFi();
  }

  Serial.println("\n====================================");
  Serial.println("         SETUP COMPLETE!");
  Serial.println("====================================");
  Serial.println("\nWaiting for input...\n");
  
  // Show READY briefly
  #if ENABLE_BOOT_ANIMATION
    displayManager.showIcon(ICON_SMILE, 2000);
    delay(2000);
  #else
    displayMessage("99");
    delay(2000);
  #endif
  
  // Enable idle animations
  #if ENABLE_IDLE_ANIMATIONS
    displayManager.enableIdleAnimations(true);
  #endif
  
  digitalWrite(STATUS_LED_PIN, HIGH);
  
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
  
  // Update display manager (animations, timeouts, etc.)
  displayManager.update();
  
  // Check WiFi connection (only in production mode) with throttling
  if (!DEBUG_MODE && WiFi.status() != WL_CONNECTED) {
    if ((unsigned long)(millis() - lastWiFiAttempt) >= WIFI_RETRY_INTERVAL) {
      Serial.println("\n[WiFi] Disconnected! Reconnecting...");
      lastWiFiAttempt = millis();
      connectWiFi();
    }
  }

  // Handle button presses
  handleButtons();

  // Handle potentiometer changes
  handlePotentiometer();

  // Update hold-to-activate progress bars if in progress
  if (pcShutdownInProgress) {
    updateShutdownProgress();
  }
  
  if (plexOnInProgress) {
    unsigned long elapsed = millis() - plexOnHoldStart;
    uint8_t progress = (uint8_t)((elapsed * 100) / PLEX_ON_HOLD_TIME);
    if (progress > 100) progress = 100;
    displayManager.showProgress(progress, PROGRESS_HORIZONTAL);
    displayActive = true;
    displayTimeout = millis() + DISPLAY_TIMEOUT_MS;
  }

  // Legacy display timeout check (now handled by displayManager)
  if (displayActive && (long)(millis() - displayTimeout) >= 0) {
    displayActive = false;
  }

  // Slower loop in debug mode for readable serial output
  delay(DEBUG_MODE ? 50 : 10);
}

// ========================================
// WIFI CONNECTION
// ========================================

void connectWiFi() {
  if (DEBUG_MODE) {
    Serial.println("[WiFi] Skipped in debug mode");
    return;
  }
  
  // Show WiFi connecting icon
  displayManager.showIcon(ICON_WIFI_1, 500);
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Update progress
    uint8_t progress = (attempts * 100) / WIFI_CONNECT_ATTEMPTS;
    displayManager.showProgress(progress, PROGRESS_HORIZONTAL);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Show success - strong WiFi signal
    displayManager.showIcon(ICON_WIFI_3, 1000);
    displayManager.setWiFiSignal(3);
    delay(1000);
  } else {
    Serial.println("\n[WiFi] Connection Failed!");
    displayManager.showError(2000);
    displayManager.setWiFiSignal(0);
    delay(2000);
  }
}

// ========================================
// HOME ASSISTANT COMMUNICATION
// ========================================

void sendHomeAssistantCommand(const char* entityId, const char* service, JsonDocument* data) {
  if (DEBUG_MODE) {
    // Debug mode: Show what would be sent
    Serial.println("  → [DEBUG] Would send to Home Assistant:");
    
    // Determine domain from entity_id
    String entityStr = String(entityId);
    int dotIndex = entityStr.indexOf('.');
    String domain = entityStr.substring(0, dotIndex);
    String url = String(HA_URL) + "/api/services/" + domain + "/" + service;
    
    Serial.print("      URL: ");
    Serial.println(url);
    
    // Create JSON payload with explicit size
    StaticJsonDocument<256> payload;
    payload["entity_id"] = entityId;
    
    if (data != nullptr) {
      JsonObject dataObj = data->as<JsonObject>();
      for (JsonPair kv : dataObj) {
        payload[kv.key()] = kv.value();
      }
    }
    
    String jsonString;
    serializeJson(payload, jsonString);
    Serial.print("      Payload: ");
    Serial.println(jsonString);
    Serial.println();
    
    // Visual feedback
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(100);
    digitalWrite(STATUS_LED_PIN, HIGH);
    return;
  }
  
  // Production mode: Actually send the command
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] Cannot send command - WiFi not connected");
    return;
  }

  HTTPClient http;
  
  // Construct URL based on service type
  String url = String(HA_URL) + "/api/services/";
  
  // Determine domain from entity_id
  String entityStr = String(entityId);
  int dotIndex = entityStr.indexOf('.');
  String domain = entityStr.substring(0, dotIndex);
  
  url += domain + "/" + service;
  
  Serial.print("[HA] Sending command to: ");
  Serial.println(url);
  Serial.print("[HA] Entity: ");
  Serial.println(entityId);

  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload with explicit size
  StaticJsonDocument<256> payload;
  payload["entity_id"] = entityId;
  
  // Add any additional data if provided
  if (data != nullptr) {
    JsonObject dataObj = data->as<JsonObject>();
    for (JsonPair kv : dataObj) {
      payload[kv.key()] = kv.value();
    }
  }

  String jsonString;
  serializeJson(payload, jsonString);
  
  Serial.print("[HA] Payload: ");
  Serial.println(jsonString);

  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    Serial.print("[HA] HTTP Response code: ");
    Serial.println(httpCode);
    
    String response = http.getString();
    if (response.length() > 0) {
      Serial.print("[HA] Response: ");
      Serial.println(response);
    }
    
    if (httpCode == 200 || httpCode == 201) {
      Serial.println("[HA] Command sent successfully!");
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(50);
      digitalWrite(STATUS_LED_PIN, HIGH);
      
      // Visual feedback - show action icon again on success (not checkmark)
      // Icon already shown before command, just keep it visible
    } else {
      // Visual feedback - error
      displayManager.showError();
    }
  } else {
    Serial.print("[HA] Error sending command: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  // Always cleanup HTTP connection
  http.end();
}

// ========================================
// DISPLAY FUNCTIONS
// ========================================

// Rotate display buffer 90° counterclockwise
void rotateDisplayCCW() {
  MD_MAX72XX* mx = myDisplay.getGraphicObject();
  for (int i = 0; i < 3; i++) {     // three clockwise rotations = one counterclockwise
    mx->transform(MD_MAX72XX::TRC);
  }
  mx->update();
}

void displayMessage(const char* msg) {
  myDisplay.displayClear();
  myDisplay.setTextAlignment(PA_LEFT);
  myDisplay.print(msg);
  
  // Apply rotation after drawing
  rotateDisplayCCW();
}

void clearProgressBar() {
  displayManager.clear();
}

void updateShutdownProgress() {
  unsigned long elapsed = millis() - pcShutdownHoldStart;
  
  // Calculate progress percentage
  uint8_t progress = (uint8_t)((elapsed * 100) / PC_SHUTDOWN_HOLD_TIME);
  if (progress > 100) progress = 100;
  
  // Update progress bar using display manager with spiral animation
  displayManager.showProgress(progress, PROGRESS_SPIRAL);
  
  // Keep display active during progress
  displayActive = true;
  displayTimeout = millis() + DISPLAY_TIMEOUT_MS;
}

// ========================================
// INPUT HANDLING
// ========================================

void handleButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(buttons[i].pin);
    
    // Check if button state changed
    if (reading != buttons[i].lastState) {
      buttons[i].lastDebounceTime = millis();
    }
    
    // If enough time has passed, consider it a valid state change
    if ((millis() - buttons[i].lastDebounceTime) > DEBOUNCE_DELAY) {
      // If the state has changed
      if (reading != buttons[i].currentState) {
        buttons[i].currentState = reading;
        
        if (buttons[i].type == MOMENTARY_BUTTON) {
          // Special handling for PC Shutdown button (by entity ID)
          if (strcmp(buttons[i].entityId, ENTITY_PC_SHUTDOWN) == 0) {
            if (buttons[i].currentState == LOW) {
              // Button pressed - start hold timer
              if (!pcShutdownInProgress) {
                pcShutdownInProgress = true;
                pcShutdownHoldStart = millis();
                clearProgressBar();
                Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                Serial.println("[PC SHUTDOWN] Button pressed - hold for 3s");
                Serial.print("  Entity: ");
                Serial.println(buttons[i].entityId);
              }
            } else {
              // Button released - check if held long enough
              if (pcShutdownInProgress) {
                unsigned long holdDuration = millis() - pcShutdownHoldStart;
                
                if (holdDuration >= PC_SHUTDOWN_HOLD_TIME) {
                  Serial.println("  → [PC SHUTDOWN] Hold complete - sending shutdown command");
                  sendHomeAssistantCommand(buttons[i].entityId, buttons[i].service);
                } else {
                  Serial.print("  → [PC SHUTDOWN] Cancelled (held ");
                  Serial.print(holdDuration);
                  Serial.println("ms < 3000ms)");
                }
                
                clearProgressBar();
                pcShutdownInProgress = false;
              }
            }
          }
          // Special handling for Plex On button (by entity ID)
          else if (strcmp(buttons[i].entityId, ENTITY_PLEX) == 0) {
            if (buttons[i].currentState == LOW) {
              // Button pressed - start hold timer
              if (!plexOnInProgress) {
                plexOnInProgress = true;
                plexOnHoldStart = millis();
                clearProgressBar();
                Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                Serial.println("[PLEX ON] Button pressed - hold for 3s");
                Serial.print("  Entity: ");
                Serial.println(buttons[i].entityId);
              }
            } else {
              // Button released - check if held long enough
              if (plexOnInProgress) {
                unsigned long holdDuration = millis() - plexOnHoldStart;
                
                if (holdDuration >= PLEX_ON_HOLD_TIME) {
                  Serial.println("  → [PLEX ON] Hold complete - sending command");
                  
                  // Visual feedback - quick flash + action icon
                  #if ENABLE_BUTTON_FLASH
                    displayManager.flash(50);
                  #endif
                  displayManager.showActionIcon(buttonNames[i]);
                  
                  sendHomeAssistantCommand(buttons[i].entityId, buttons[i].service);
                } else {
                  Serial.print("  → [PLEX ON] Cancelled (held ");
                  Serial.print(holdDuration);
                  Serial.println("ms < 3000ms)");
                }
                
                clearProgressBar();
                plexOnInProgress = false;
              }
            }
          } else {
            // Normal momentary button: only trigger on press (HIGH to LOW transition)
            if (buttons[i].currentState == LOW) {
              Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
              Serial.print("[BUTTON] ");
              Serial.print(buttonNames[i]);
              Serial.print(" (GPIO ");
              Serial.print(buttons[i].pin);
              Serial.println(")");
              Serial.println("  Action: PRESSED");
              Serial.print("  Entity: ");
              Serial.println(buttons[i].entityId);
              Serial.print("  Service: ");
              Serial.println(buttons[i].service);
              
              // Visual feedback - quick flash + action icon
              #if ENABLE_BUTTON_FLASH
                displayManager.flash(50);
              #endif
              displayManager.showActionIcon(buttonNames[i]);
              
              sendHomeAssistantCommand(buttons[i].entityId, buttons[i].service);
            }
          }
        } else {
          // Toggle switch: send on/off based on position
          Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
          Serial.print("[TOGGLE SWITCH] ");
          Serial.print(buttonNames[i]);
          Serial.print(" (GPIO ");
          Serial.print(buttons[i].pin);
          Serial.println(")");
          
          // LOW = switch in ON position, HIGH = switch in OFF position
          const char* service;
          const char* position;
          if (buttons[i].currentState == LOW) {
            service = "turn_on";
            position = "ON";
          } else {
            service = "turn_off";
            position = "OFF";
          }
          
          Serial.print("  Position: ");
          Serial.println(position);
          Serial.print("  Entity: ");
          Serial.println(buttons[i].entityId);
          Serial.print("  Service: ");
          Serial.println(service);
          
          // Visual feedback for toggle switches
          #if ENABLE_BUTTON_FLASH
            displayManager.flash(50);
          #endif
          displayManager.showActionIcon(buttonNames[i]);
          
          // Special handling for AC Bypass switch (by entity ID)
          if (strcmp(buttons[i].entityId, ENTITY_AC_BYPASS) == 0) {
            Serial.println("  [AC BYPASS]");
            
            if (buttons[i].currentState == LOW) {
              // Switch ON = Turn on bypass boolean
              Serial.println("  → Turning on AC bypass");
              sendHomeAssistantCommand(ENTITY_AC_BYPASS, "turn_on");
            } else {
              // Switch OFF = Trigger bypass-off automation
              Serial.println("  → Triggering bypass-off automation");
              sendHomeAssistantCommand(ENTITY_AC_AUTOMATION, "trigger");
            }
          } else {
            // Normal toggle switch - single entity
            sendHomeAssistantCommand(buttons[i].entityId, service);
          }
        }
      }
    }
    
    buttons[i].lastState = reading;
  }
}

void handlePotentiometer() {
  // Read raw value and add to rolling average
  potTotal -= potReadings[potReadIndex];
  potReadings[potReadIndex] = analogRead(POT_AC_TEMP_PIN);
  potTotal += potReadings[potReadIndex];
  potReadIndex = (potReadIndex + 1) % POT_SAMPLES;
  
  // Calculate averaged ADC value
  int avgRawValue = potTotal / POT_SAMPLES;
  
  // Map to temperature range (inverted for upside-down pot installation)
  int potValue = map(avgRawValue, 0, ADC_MAX_VALUE, TEMP_MAX, TEMP_MIN);
  
  // Check if value has changed
  if (potValue != potStableValue) {
    // Value changed - reset stability timer
    potStableValue = potValue;
    potStableTime = millis();
    
    // Update display immediately for responsive feel
    currentSetpoint = potValue;
    displayManager.showTemperature(potValue);
    return;
  }
  
  // Value is stable - check if it's been stable long enough
  if (potValue == potStableValue &&
      (millis() - potStableTime) >= POT_STABLE_DURATION &&
      potValue != lastPotValue) {
    
    // Value has been stable for required duration and is different from last sent value
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("[POTENTIOMETER] Temperature Setpoint Changed");
    Serial.print("  Averaged ADC: ");
    Serial.print(avgRawValue);
    Serial.println(" / 4095");
    Serial.print("  Stable Temp: ");
    Serial.print(potValue);
    Serial.print("°C (Range: ");
    Serial.print(TEMP_MIN);
    Serial.print("-");
    Serial.print(TEMP_MAX);
    Serial.println("°C)");
    
    // Send temperature setpoint to Home Assistant
    StaticJsonDocument<128> data;
    data["value"] = potValue;
    
    sendHomeAssistantCommand(ENTITY_AC_TEMP, "set_value", &data);
    
    lastPotValue = potValue;
    currentSetpoint = potValue;
    lastPotSendTime = millis();
    
    // Keep showing temperature (no checkmark needed)
    displayManager.showTemperature(potValue);
  }
}