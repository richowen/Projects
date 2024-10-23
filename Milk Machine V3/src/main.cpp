#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "laser_sensor.h"
#include "water_level_sensor.h"
#include <Wire.h>

// Global variables
State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long mixingStartTime = 0;
bool sensorActivatedDuringPostMix = false;
int hopperLevel = 0;
unsigned long lastLCDUpdate = 0;
State lastReportedState = IDLE;
int lastReportedHopperLevel = -1;
uint8_t currentErrors = 0;
const uint8_t ERROR_HOPPER_LOW = 0x01;
const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02;

// Replace old liquid sensor with new water level sensor
WaterLevelSensor waterSensor;

// Function declarations
void setupPins();
void handleIdleState();
void handleMixingState();
void handleWaitingPostMixState();
void handleErrorState();
void handleWashStandbyState();
void handleWashDispenseState();
void transitionTo(State newState);
const char* getStateString(State state);
void updateDevices(State state);
void updateLCDWithSensorInfo();
State readWashMode();
bool checkForErrors();

// Function Implementations
void setupPins() {
    // Configure output pins
    pinMode(mixerPin, OUTPUT);
    pinMode(waterPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(agitatorPin, OUTPUT);

    // Configure input pins
    pinMode(washStandbyPin, INPUT_PULLUP);
    pinMode(washDispensePin, INPUT_PULLUP);

    // Initialize all outputs to OFF (HIGH for active low outputs)
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
}

void handleIdleState() {
    if (waterSensor.update() && waterSensor.isReliable()) {
        uint8_t level = waterSensor.getLevel();
        
        // Start mixing if bowl is empty and hopper has powder
        if (level < 10 && !isHopperLow()) {
            transitionTo(MIXING);
        } else if (isHopperLow()) {
            currentErrors |= ERROR_HOPPER_LOW;
            transitionTo(ERROR);
        }
    }
}

void handleMixingState() {
    if (waterSensor.update() && waterSensor.isReliable()) {
        uint8_t level = waterSensor.getLevel();
        
        // Stop mixing when bowl is full
        if (level > 90) {
            sensorActivatedDuringPostMix = false;
            transitionTo(WAITING_POST_MIX);
        } 
        else if (millis() - mixingStartTime >= maxMixingDuration) {
            currentErrors |= ERROR_MIX_TIME_EXCEEDED;
            transitionTo(ERROR);
        } 
        else if (isHopperLow()) {
            currentErrors |= ERROR_HOPPER_LOW;
            transitionTo(ERROR);
        }
    }
}

void handleWaitingPostMixState() {
    if (waterSensor.update() && waterSensor.isReliable()) {
        uint8_t level = waterSensor.getLevel();
        
        // If level drops during post-mix waiting, note it
        if (level < 10) {
            sensorActivatedDuringPostMix = true;
        }
    }

    if (millis() - stateStartTime >= waitingDuration) {
        if (sensorActivatedDuringPostMix) {
            transitionTo(MIXING);
        } else {
            transitionTo(IDLE);
        }
    }
}

void handleWashStandbyState() {
    // In wash standby, all outputs are off
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
}

void handleWashDispenseState() {
    // In wash dispense, only water is on
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, LOW);   // Water on
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
}

void handleErrorState() {
    // Turn off all outputs in error state
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);

    // Update LCD with error information
    clearLCD();
    setCursor(0, 0);
    printLCD("ERROR:");
    setCursor(0, 1);

    if (currentErrors & ERROR_HOPPER_LOW) {
        printLCD("Low Hopper");
    }
    if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
        printLCD("Mix Time Exceeded");
    }

    // Check if errors are cleared
    uint8_t newErrors = 0;
    if (isHopperLow()) {
        newErrors |= ERROR_HOPPER_LOW;
    }
    if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
        newErrors |= ERROR_MIX_TIME_EXCEEDED;
    }

    currentErrors = newErrors;

    if (currentErrors == 0) {
        transitionTo(IDLE);
    }
}

void updateLCDWithSensorInfo() {
    clearLCD();
    setCursor(0, 0);
    printLCD(getStateString(currentState));

    setCursor(0, 1);
    printLCD("H:");
    printLCD(hopperLevel);
    printLCD("% W:");
    if (waterSensor.isReliable()) {
        printLCD(waterSensor.getLevel());
        printLCD("%");
    } else {
        printLCD("ERR");
    }
}

bool checkForErrors() {
    uint8_t newErrors = 0;

    // Check for low hopper level
    if (isHopperLow()) {
        newErrors |= ERROR_HOPPER_LOW;
    }

    // Check for exceeding max mixing time
    if (currentState == MIXING && (millis() - mixingStartTime >= maxMixingDuration)) {
        newErrors |= ERROR_MIX_TIME_EXCEEDED;
    }

    // Only update currentErrors if we're not already in an error state
    if (currentState != ERROR) {
        currentErrors = newErrors;
    }

    return currentErrors != 0;
}

// Update the transitionTo() function to use the new debug macros:
void transitionTo(State newState) {
    currentState = newState;
    stateStartTime = millis();

    if (newState == MIXING) {
        mixingStartTime = millis();
    }

    const char* stateStr = getStateString(newState);
    DEBUG_PRINTF("Transitioning to state: %s\n", stateStr);
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);
}

const char* getStateString(State state) {
    switch (state) {
        case IDLE:
            return "idle";
        case MIXING:
            return "mixing";
        case WAITING_POST_MIX:
            return "waiting_post_mix";
        case ERROR:
            return "error";
        case WASH_STANDBY:
            return "wash_standby";
        case WASH_DISPENSE:
            return "wash_dispense";
        default:
            return "unknown";
    }
}

State readWashMode() {
    bool standbyActive = digitalRead(washStandbyPin) == LOW;
    bool dispenseActive = digitalRead(washDispensePin) == LOW;

    if (standbyActive && dispenseActive) {
        return WASH_DISPENSE;
    } else if (standbyActive) {
        return WASH_STANDBY;
    }
    return IDLE;
}

void updateDevices(State state) {
    switch (state) {
        case ERROR:
        case IDLE:
        case WASH_STANDBY:
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
        case WASH_DISPENSE:
            digitalWrite(mixerPin, HIGH);
            digitalWrite(waterPin, LOW);
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
    }
}

void setup() {
    // Initialize Serial with timeout
    Serial.begin(115200);
    
    // Only wait for serial if in debug mode and with timeout
    #ifdef DEBUG
        unsigned long serialTimeout = millis();
        while (!Serial && (millis() - serialTimeout < 1000)) {
            // Wait up to 1 second for serial connection
            delay(1);
        }
    #endif

    // Debug messages only printed if Serial is actually available
    if (Serial) {
        Serial.println("Serial communication initialized");
    }

    Wire.begin(); // Initialize I2C
    delay(100);   // Give some time for I2C bus to stabilize

    setupPins();
    if (Serial) Serial.println("Pins setup complete");

    setupLCD();
    if (Serial) Serial.println("LCD setup complete");

    setupLaserSensor();
    if (Serial) Serial.println("Laser sensor setup complete");

    waterSensor.begin();
    if (Serial) Serial.println("Water level sensor setup complete");

    setupWiFi();
    if (Serial) Serial.println("WiFi setup complete");

    setupHomeAssistant();
    if (Serial) Serial.println("Home Assistant setup complete");

    if (Serial) Serial.println("Setup complete");

    // Show initialization complete on LCD
    displayMessage("System Ready");
    delay(1000);  // Show message briefly
}

void loop() {
    loopHomeAssistant();
    hopperLevel = readHopperLevel();

    // Always check for errors
    if (checkForErrors()) {
        if (currentState != ERROR) {
            transitionTo(ERROR);
        }
        handleErrorState();
        return;
    }

    State washMode = readWashMode();
    switch (washMode) {
        case IDLE:
            if (currentState == WASH_STANDBY || currentState == WASH_DISPENSE) {
                transitionTo(IDLE);
            } else {
                // Normal operation with simplified state machine
                switch (currentState) {
                    case IDLE:
                        handleIdleState();
                        break;
                    case MIXING:
                        handleMixingState();
                        break;
                    case WAITING_POST_MIX:
                        handleWaitingPostMixState();
                        break;
                }
            }
            break;
        case WASH_STANDBY:
            if (currentState != WASH_STANDBY) {
                transitionTo(WASH_STANDBY);
            }
            handleWashStandbyState();
            break;
        case WASH_DISPENSE:
            if (currentState != WASH_DISPENSE) {
                transitionTo(WASH_DISPENSE);
            }
            handleWashDispenseState();
            break;
    }

    // Update display routines
    unsigned long currentMillis = millis();
    if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState, hopperLevel);
        lastLCDUpdate = currentMillis;

        #ifdef DEBUG_WATER_LEVEL
            waterSensor.printDebug();
        #endif
    }

    if (currentState != lastReportedState || hopperLevel != lastReportedHopperLevel) {
        updateHomeAssistant(getStateString(currentState), hopperLevel);
        lastReportedState = currentState;
        lastReportedHopperLevel = hopperLevel;
    }
}