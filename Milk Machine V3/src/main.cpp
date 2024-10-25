#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "laser_sensor.h"
#include "water_level_sensor.h"
#include "debug_utils.h"
#include "error_handler.h"
#include <WiFi.h>
#include <WiFiClient.h>

// ----------------- Global Variables -----------------

// State function pointer
typedef void (*StateHandler)();
StateHandler currentStateHandler = nullptr;

// Error flags
const uint8_t ERROR_HOPPER_LOW = 0x01;
const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02;
const uint8_t ERROR_WATER_SENSOR_FAILURE = 0x03;

State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDebugOutput = 0;  // New: For periodic debug output
int hopperLevel = 0;
bool sensorActivatedDuringPostMix = false;
uint8_t currentErrors = 0;

// Sensor objects
LaserSensor laserSensor;         // Laser sensor object
WaterLevelSensor waterSensor(waterBottomPin, waterTopPin);    // Water level sensor object
StateHandler getStateHandler(State state);

// Function declarations
void setupPins();
void updateSensors();
void checkForErrors();
void transitionTo(State newState, StateHandler newStateHandler);
void executeCurrentState();
void handleIdleState();
void handleMixingState();
void handleWaitingPostMixState();
void handleWashStandbyState();
void handleWashDispenseState();
void handleErrorState();
State readWashMode();
void updateDevices(State state);
const char* getStateString(State state);
void handleTelnet();
void printDebugInfo();

// ----------------- Setup and Loop -----------------

void setup() {
    Serial.begin(115200);
    Wire.begin();
    delay(100);  // Allow I2C to stabilize

    Serial.println("\nInitializing Milk Mixer System...");

    setupPins();
    Serial.println("Pins initialized");
    
    setupLCD();
    Serial.println("LCD initialized");

    // Initialize laser sensor
    if (!laserSensor.begin()) {
        Serial.println("ERROR: Laser sensor initialization failed!");
    } else {
        Serial.println("Laser sensor initialized successfully");
    }

    // Initialize water sensor
    waterSensor.begin();
    Serial.println("Water sensor initialized");

    // Connect to WiFi
    setupWiFi();

    // Wait for WiFi connection before proceeding
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Setup Home Assistant integration
    setupHomeAssistant();
    Serial.println("Home Assistant integration initialized");

    // **Initialize Telnet server using setupTelnet()**
    setupTelnet();

    // Initial state
    transitionTo(IDLE, handleIdleState);
    displayMessage("System Ready");

    // Initial sensor readings
    updateSensors();
    Serial.println("Initial water level: " + String(waterSensor.getLevel()));
    Serial.println("Initial hopper level: " + String(hopperLevel) + "%");

    errorHandler = new ErrorHandler(laserSensor, waterSensor);
    errorHandler->begin();
    
    delay(1000);
}

// ----------------- Main Loop -----------------
void loop() {
    // 1. Update sensor readings every second
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate >= 1000) {
        updateSensors();
        lastSensorUpdate = millis();
    }

    // 2. Check for wash mode changes
    State washMode = readWashMode();
    if (washMode != currentState && (washMode == WASH_STANDBY || washMode == WASH_DISPENSE)) {
        transitionTo(washMode, getStateHandler(washMode));
    } else if ((currentState == WASH_STANDBY || currentState == WASH_DISPENSE) && washMode == IDLE) {
        transitionTo(IDLE, handleIdleState);
    }

    // 3. Check for errors and handle states
    errorHandler->handle(currentState);
    
    if (errorHandler->hasErrors() && currentState != ERROR) {
        transitionTo(ERROR, handleErrorState);
    } else if (!errorHandler->hasErrors() && currentState == ERROR) {
        transitionTo(IDLE, handleIdleState);
    } else if (!errorHandler->hasErrors()) {
        executeCurrentState();
    }

    // 4. Update displays and communications
    static unsigned long lastLCDUpdate = 0;
    if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState, hopperLevel);
        lastLCDUpdate = millis();
    }

    loopHomeAssistant();
    handleTelnet();
}

// ----------------- State Management -----------------
void executeCurrentState() {
    if (currentStateHandler) {
        currentStateHandler();
    }
}

void transitionTo(State newState, StateHandler newStateHandler) {
    if (currentState == newState) {
        return;
    }

    Serial.printf("State transition: %s -> %s\n", getStateString(currentState), getStateString(newState));
    
    currentState = newState;
    currentStateHandler = newStateHandler;
    stateStartTime = millis();
    
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);
    updateHomeAssistant(getStateString(newState), hopperLevel);
}

StateHandler getStateHandler(State state) {
    switch (state) {
        case IDLE: return handleIdleState;
        case MIXING: return handleMixingState;
        case WAITING_POST_MIX: return handleWaitingPostMixState;
        case WASH_STANDBY: return handleWashStandbyState;
        case WASH_DISPENSE: return handleWashDispenseState;
        case ERROR: return handleErrorState;
        default: return handleIdleState;
    }
}

// ----------------- Sensor and Error Management -----------------

void updateSensors() {
    // Update hopper level
    int previousHopperLevel = hopperLevel;
    hopperLevel = laserSensor.readHopperLevel();
    if (abs(hopperLevel - previousHopperLevel) > 10) {
        Serial.printf("Significant hopper level change: %d -> %d\n", previousHopperLevel, hopperLevel);
    }

    // Update water level sensor and handle its states
    WaterLevelSensor::Level waterLevel = waterSensor.update();

    updateHomeAssistant(getStateString(currentState), hopperLevel);
    }

// ----------------- State Handlers -----------------

void handleIdleState() {
    WaterLevelSensor::Level waterLevel = waterSensor.getLevel();
    
    if (waterLevel == WaterLevelSensor::EMPTY && !laserSensor.isHopperLow()) {
        Serial.println("Water level EMPTY, transitioning to MIXING");
        transitionTo(MIXING, handleMixingState);
    }
}

void handleMixingState() {
    WaterLevelSensor::Level waterLevel = waterSensor.getLevel();
    
    if (waterLevel == WaterLevelSensor::FULL) {
        Serial.println("Water level FULL, transitioning to WAITING_POST_MIX");
        sensorActivatedDuringPostMix = false;
        transitionTo(WAITING_POST_MIX, handleWaitingPostMixState);
    }
}

void handleWaitingPostMixState() {
    WaterLevelSensor::Level waterLevel = waterSensor.getLevel();
    
    if (waterLevel == WaterLevelSensor::EMPTY) {
        Serial.println("Water level dropped during post-mix waiting period");
        sensorActivatedDuringPostMix = true;
    }

    if (millis() - stateStartTime >= waitingDuration) {
        if (sensorActivatedDuringPostMix) {
            Serial.println("Post-mix wait complete - returning to MIXING due to sensor activation");
            transitionTo(MIXING, handleMixingState);
        } else {
            Serial.println("Post-mix wait complete - returning to IDLE");
            transitionTo(IDLE, handleIdleState);
        }
    }
}

void handleWashStandbyState() {
    // Devices are already set in updateDevices()
}

void handleWashDispenseState() {
    // Devices are already set in updateDevices()
}

void handleErrorState() {
    errorHandler->handleErrorState();
    
    // If errors are cleared, transition back to IDLE
    if (!errorHandler->hasErrors()) {
        transitionTo(IDLE, handleIdleState);
    }
}

// ----------------- Debug Information -----------------

void printDebugInfo() {
    debugPrintln("\n=== System Status ===");

    char buffer[60];

    // Current state and uptime
    snprintf(buffer, sizeof(buffer), "State: %s, Uptime: %lus", 
             getStateString(currentState), millis() / 1000);
    debugPrintln(buffer);

    // Water level state and hopper level
    const char* waterLevelStr;
    switch (waterSensor.getLevel()) {
        case WaterLevelSensor::EMPTY: waterLevelStr = "EMPTY"; break;
        case WaterLevelSensor::PARTIAL: waterLevelStr = "PARTIAL"; break;
        case WaterLevelSensor::FULL: waterLevelStr = "FULL"; break;
        default: waterLevelStr = "ERROR"; break;
    }
    
    snprintf(buffer, sizeof(buffer), "Water Level: %s, Hopper Level: %d%%", 
             waterLevelStr, hopperLevel);
    debugPrintln(buffer);

    // Error status summary
    snprintf(buffer, sizeof(buffer), "Errors: 0x%02X", currentErrors);
    debugPrintln(buffer);

    // Short summary of free heap memory and Wi-Fi signal strength
    snprintf(buffer, sizeof(buffer), "Heap: %luB, WiFi RSSI: %ddBm", 
             ESP.getFreeHeap(), WiFi.RSSI());
    debugPrintln(buffer);

    debugPrintln("==================\n");
}

// ----------------- Pin Setup -----------------

void setupPins() {
    pinMode(mixerPin, OUTPUT);
    pinMode(waterPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(agitatorPin, OUTPUT);
    pinMode(washStandbyPin, INPUT_PULLUP);
    pinMode(washDispensePin, INPUT_PULLUP);
    pinMode(resetSwitchPin, INPUT_PULLUP);

    // Set initial pin states
    digitalWrite(mixerPin, HIGH);    // Active LOW
    digitalWrite(waterPin, HIGH);    // Active LOW
    digitalWrite(augerPin, HIGH);    // Active LOW
    digitalWrite(agitatorPin, HIGH); // Active LOW
    
    Serial.println("Pins initialized to safe state");
}

// ----------------- Update Devices Based on State -----------------

void updateDevices(State state) {
    switch (state) {
        case IDLE:
        case ERROR:
            digitalWrite(mixerPin, HIGH);
            digitalWrite(waterPin, HIGH);
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
        case MIXING:
            digitalWrite(mixerPin, LOW);
            digitalWrite(waterPin, LOW);
            digitalWrite(augerPin, LOW);
            digitalWrite(agitatorPin, LOW);
            break;
        case WAITING_POST_MIX:
            digitalWrite(mixerPin, LOW);
            digitalWrite(waterPin, HIGH);
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
        case WASH_STANDBY:
            digitalWrite(mixerPin, HIGH);
            digitalWrite(waterPin, HIGH);
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
        case WASH_DISPENSE:
            digitalWrite(mixerPin, HIGH);
            digitalWrite(waterPin, LOW);  // Water on
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
    }
}

// ----------------- Wash Mode Reading -----------------

State readWashMode() {
    bool standbyActive = digitalRead(washStandbyPin) == LOW;   // Active LOW
    bool dispenseActive = digitalRead(washDispensePin) == LOW; // Active LOW

    if (standbyActive && dispenseActive) {
        return WASH_DISPENSE;
    } else if (standbyActive) {
        return WASH_STANDBY;
    }
    return IDLE;
}

// ----------------- Utility Functions -----------------

const char* getStateString(State state) {
    switch (state) {
        case IDLE: return "IDLE";
        case MIXING: return "MIXING";
        case WAITING_POST_MIX: return "WAITING_POST_MIX";
        case WASH_STANDBY: return "WASH_STANDBY";
        case WASH_DISPENSE: return "WASH_DISPENSE";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
