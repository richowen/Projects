#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "water_level_sensor.h"
#include "debug_utils.h"
#include "error_handler.h"
#include <WiFi.h>
#include <WiFiClient.h>

// ----------------- Global Variables -----------------

// State function pointer
typedef void (*StateHandler)();
StateHandler currentStateHandler = nullptr;
ErrorHandler* errorHandler = nullptr;

State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDebugOutput = 0;
bool sensorActivatedDuringPostMix = false;

// Sensor objects
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
    delay(100);

    Serial.println("\nInitializing Milk Mixer System...");

    setupPins();
    Serial.println("Pins initialized");
    
    setupLCD();
    Serial.println("LCD initialized");

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

    setupTelnet();

    // Create error handler with water sensor
    errorHandler = new ErrorHandler(waterSensor);
    errorHandler->begin();

    // Important: Give sensors time to stabilize before entering initial state
    delay(500);
    waterSensor.update();  // Initial reading
    delay(100);  // Short delay
    waterSensor.update();  // Second reading to ensure stability
    
    // Initial state
    transitionTo(IDLE, handleIdleState);
    displayMessage("System Ready");
    
    // Debug output of initial state
    debugPrintln("Initial system state:");
    printDebugInfo();
}

// ----------------- Main Loop -----------------
void loop() {
    // 1. Update sensor readings every second
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

    if (errorHandler->hasErrors()) {
        if (currentState != ERROR) {
            transitionTo(ERROR, handleErrorState);
        }
    } else {
        // No errors, proceed with normal state execution
        executeCurrentState();
    }

    // 4. Update displays and communications
    static unsigned long lastLCDUpdate = 0;
    if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState);
        lastLCDUpdate = millis();
    }

    // 5. Debug output
    static unsigned long lastDebugUpdate = 0;
    if (millis() - lastDebugUpdate >= DEBUG_UPDATE_INTERVAL) {
        printDebugInfo();
        lastDebugUpdate = millis();
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

    debugPrintf("State transition: %s -> %s\n", getStateString(currentState), getStateString(newState));
    
    // Handle mix timer state transitions
    if (currentState == MIXING) {
        if (newState == ERROR) {
            // Keep timer active if transitioning to error during mixing
            mixTimerActive = true;
        } else {
            // Reset timer when leaving mixing state for any other state
            mixStartTime = 0;
            mixTimerActive = false;
            debugPrintln("Mix timer reset");
        }
    }
    
    currentState = newState;
    currentStateHandler = newStateHandler;
    stateStartTime = millis();
    
    updateLCD(currentState);
    updateDevices(newState);
    updateHomeAssistant(getStateString(newState));
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
    // Just update water level sensor
    WaterLevelSensor::Level waterLevel = waterSensor.update();
    
    // Update Home Assistant
    updateHomeAssistant(getStateString(currentState));
}

// ----------------- State Handlers -----------------

void handleIdleState() {
    static unsigned long lastIdleCheck = 0;
    static const unsigned long IDLE_CHECK_INTERVAL = 100; // Check every 100ms

    if (millis() - lastIdleCheck >= IDLE_CHECK_INTERVAL) {
        lastIdleCheck = millis();
        
        if (waterSensor.getLevel() == WaterLevelSensor::EMPTY) {
            debugPrintln("Water level EMPTY detected in IDLE, transitioning to MIXING");
            transitionTo(MIXING, handleMixingState);
        }
    }
}

void handleMixingState() {
    // Only start mix timer when first entering mixing state
    if (!mixTimerActive) {
        mixStartTime = millis();
        mixTimerActive = true;
        debugPrintf("Starting mix timer at: %lu\n", mixStartTime);
    }

    // Check for mix time exceeded
    if (millis() - mixStartTime >= maxMixingDuration) {
        debugPrintln("Mix time exceeded maximum duration");
        errorHandler->check(currentState);
        return;
    }

    if (waterSensor.getLevel() == WaterLevelSensor::FULL) {
        debugPrintln("Water level FULL, transitioning to WAITING_POST_MIX");
        sensorActivatedDuringPostMix = false;
        transitionTo(WAITING_POST_MIX, handleWaitingPostMixState);
    }
}

void handleWaitingPostMixState() {
    if (waterSensor.getLevel() == WaterLevelSensor::EMPTY) {
        debugPrintln("Water level dropped during post-mix waiting period");
        sensorActivatedDuringPostMix = true;
    }

    if (millis() - stateStartTime >= waitingDuration) {
        if (sensorActivatedDuringPostMix) {
            debugPrintln("Post-mix wait complete - returning to MIXING due to sensor activation");
            transitionTo(MIXING, handleMixingState);
        } else {
            debugPrintln("Post-mix wait complete - returning to IDLE");
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
// ----------------- Pin Setup -----------------

void setupPins() {
    pinMode(mixerPin, OUTPUT);
    pinMode(waterPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(agitatorPin, OUTPUT);
    pinMode(washStandbyPin, INPUT_PULLUP);
    pinMode(washDispensePin, INPUT_PULLUP);

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
