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

// Core system components
WaterLevelSensor waterSensor(liquidLevelPin);
ErrorHandler* errorHandler = nullptr;
State currentState = IDLE;

// Function declarations
void setupPins();
void updateSystem();
void handleState();
void transitionTo(State newState);
void updateOutputs();
State getWashMode();
const char* getStateString(State state);

void setup() {
    // Initialize serial first for debug output
    Serial.begin(115200);
    Wire.begin();
    Serial.println("\nMilk Mixer System Starting...");

    // Initialize core components
    setupPins();
    setupLCD();
    waterSensor.begin();
    
    // Setup network components
    setupWiFi();
    setupHomeAssistant();
    setupTelnet();

    // Initialize error handling
    errorHandler = new ErrorHandler(waterSensor);
    errorHandler->begin();

    // Allow sensors to stabilize
    delay(500);
    waterSensor.update();
    
    // Set initial state
    transitionTo(IDLE);
    displayMessage("System Ready");
    printDebugInfo();
}

void loop() {
    // Fixed interval updates
    if (millis() - lastUpdate >= UPDATE_INTERVAL) {
        updateSystem();
        lastUpdate = millis();
    }

    // Always-running background tasks
    loopHomeAssistant();
    handleTelnet();
}

void setupPins() {
    // Configure output pins
    const int outputPins[] = {mixerPin, waterPin, augerPin, agitatorPin};
    for (int pin : outputPins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH); // Active LOW, so HIGH is off
    }

    // Configure input pins
    const int inputPins[] = {washStandbyPin, washDispensePin};
    for (int pin : inputPins) {
        pinMode(pin, INPUT_PULLUP);
    }
}

void updateSystem() {
    // 1. Update sensors
    waterSensor.update();

    // 2. Check for wash mode changes
    State washMode = getWashMode();
    if (washMode != currentState && (washMode == WASH_STANDBY || washMode == WASH_DISPENSE)) {
        transitionTo(washMode);
    } else if ((currentState == WASH_STANDBY || currentState == WASH_DISPENSE) && washMode == IDLE) {
        transitionTo(IDLE);
    }

    // 3. Handle errors
    errorHandler->handle(currentState);
    if (errorHandler->hasErrors()) {
        if (currentState != ERROR) {
            transitionTo(ERROR);
        }
        return;
    }

    // 4. Handle current state
    handleState();

    // 5. Update outputs
    updateOutputs();
    
    // 6. Update displays and logging
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState);
        printDebugInfo();
        lastDisplayUpdate = millis();
    }
}

void handleState() {
    switch (currentState) {
        case IDLE:
            if (waterSensor.getLevel() == WaterLevelSensor::EMPTY) {
                transitionTo(MIXING);
            }
            break;

        case MIXING:
            if (!mixTimerActive) {
                mixStartTime = millis();
                mixTimerActive = true;
                debugPrintln("Mix timer started");
            }
            
            if (millis() - mixStartTime >= maxMixingDuration) {
                errorHandler->check(currentState);
                return;
            }

            if (waterSensor.getLevel() == WaterLevelSensor::FULL) {
                transitionTo(WAITING_POST_MIX);
            }
            break;

        case WAITING_POST_MIX:
            static bool sensorTriggered = false;
            
            if (waterSensor.getLevel() == WaterLevelSensor::EMPTY) {
                sensorTriggered = true;
            }

            if (millis() - mixStartTime >= waitingDuration) {
                transitionTo(sensorTriggered ? MIXING : IDLE);
                sensorTriggered = false;
            }
            break;

        case ERROR:
            if (!errorHandler->hasErrors()) {
                transitionTo(IDLE);
            }
            break;

        // Wash states are handled by pin states in updateOutputs()
        case WASH_STANDBY:
        case WASH_DISPENSE:
            break;
    }
}

void transitionTo(State newState) {
    if (currentState == newState) return;

    debugPrintf("State transition: %s -> %s\n", 
                getStateString(currentState), 
                getStateString(newState));

    // Handle mix timer state changes
    if (currentState == MIXING && newState != ERROR) {
        mixTimerActive = false;
        mixStartTime = 0;
    }

    currentState = newState;
    updateOutputs();
    updateLCD(currentState);
    updateHomeAssistant(getStateString(newState));
}

void updateOutputs() {
    // All pins are active LOW
    bool mixerOn = false;
    bool waterOn = false;
    bool augerOn = false;
    bool agitatorOn = false;

    switch (currentState) {
        case MIXING:
            mixerOn = waterOn = augerOn = agitatorOn = true;
            break;
        case WAITING_POST_MIX:
            mixerOn = true;
            break;
        case WASH_DISPENSE:
            waterOn = true;
            break;
        // All other states keep everything off
    }

    digitalWrite(mixerPin, !mixerOn);     // Convert to active LOW
    digitalWrite(waterPin, !waterOn);      // Convert to active LOW
    digitalWrite(augerPin, !augerOn);      // Convert to active LOW
    digitalWrite(agitatorPin, !agitatorOn); // Convert to active LOW
}

State getWashMode() {
    bool standby = digitalRead(washStandbyPin) == LOW;   // Active LOW
    bool dispense = digitalRead(washDispensePin) == LOW; // Active LOW

    if (standby && dispense) return WASH_DISPENSE;
    if (standby) return WASH_STANDBY;
    return IDLE;
}

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