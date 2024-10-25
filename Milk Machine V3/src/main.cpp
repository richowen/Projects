#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "laser_sensor.h"
#include "water_level_sensor.h"
#include "debug_utils.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

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
WaterLevelSensor waterSensor;    // Water level sensor object

// Web Server
WebServer server(80);  // Initialize WebServer

// Function declarations
void setupPins();
void updateSensors();
void handleErrors();
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
void printDebugInfo();  // New: Debug information function

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

    // Start mDNS service
    if (!MDNS.begin("MyESP32")) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS responder started");
    }

    // Start the web server
    server.begin();
    Serial.println("HTTP server started");

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
    Serial.println("Initial water level: " + String(waterSensor.getLevel()) + "%");
    Serial.println("Initial hopper level: " + String(hopperLevel) + "%");
    
    delay(1000);
}


void loop() {

    // Check for wash mode changes
    State washMode = readWashMode();
    if ((washMode == WASH_STANDBY || washMode == WASH_DISPENSE) && washMode != currentState) {
        // Transition to wash mode states
        if (washMode == WASH_STANDBY) {
            transitionTo(WASH_STANDBY, handleWashStandbyState);
        } else if (washMode == WASH_DISPENSE) {
            transitionTo(WASH_DISPENSE, handleWashDispenseState);
        }
    } else if ((currentState == WASH_STANDBY || currentState == WASH_DISPENSE) && washMode == IDLE) {
        // Transition back to IDLE when wash mode is deactivated
        transitionTo(IDLE, handleIdleState);
    }

    // Proceed with the rest of the loop only if not in wash modes
    if (currentState != WASH_STANDBY && currentState != WASH_DISPENSE) {
        // Periodically update sensors
        if (millis() - lastSensorUpdate >= 1000) {
            updateSensors();
            lastSensorUpdate = millis();
        }

        // Debug output every 5 seconds
        if (millis() - lastDebugOutput >= 5000) {
            printDebugInfo();
            lastDebugOutput = millis();
        }

        // Process errors
        handleErrors();

        // Execute current state
        executeCurrentState();
    }

    // Periodically update the LCD
    if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState, hopperLevel);
        lastLCDUpdate = millis();
    }

    // Handle Home Assistant tasks
    loopHomeAssistant();

    // Handle web server request updates
    server.handleClient();

    // Handle Telnet communication
    handleTelnet();
}

// ----------------- State Management -----------------

void executeCurrentState() {
    if (currentStateHandler) {
        currentStateHandler();
    }
}

void transitionTo(State newState, StateHandler newStateHandler) {
    Serial.printf("State transition: %s -> %s\n", getStateString(currentState), getStateString(newState));
    currentState = newState;
    currentStateHandler = newStateHandler;
    stateStartTime = millis();
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);
    updateHomeAssistant(getStateString(newState), hopperLevel);
}

// ----------------- Sensor and Error Management -----------------

void updateSensors() {
    // Update hopper level
    int previousHopperLevel = hopperLevel;
    hopperLevel = laserSensor.readHopperLevel();
    if (abs(hopperLevel - previousHopperLevel) > 10) {
        Serial.printf("Significant hopper level change: %d -> %d\n", previousHopperLevel, hopperLevel);
    }

    // Update water level sensor
    bool waterUpdateSuccess = waterSensor.update();
    if (!waterUpdateSuccess) {
        Serial.println("WARNING: Water sensor update failed!");
    }

    // Update Home Assistant with new values
    updateHomeAssistant(getStateString(currentState), hopperLevel);
}

void checkForErrors() {
    uint8_t newErrors = 0;

    if (laserSensor.isHopperLow()) {
        newErrors |= ERROR_HOPPER_LOW;
        Serial.println("Error: Hopper low detected");
    }

    if (currentState == MIXING && (millis() - stateStartTime >= maxMixingDuration)) {
        newErrors |= ERROR_MIX_TIME_EXCEEDED;
        Serial.println("Error: Mix time exceeded");
    }

    if (newErrors != currentErrors) {
        Serial.printf("Error status changed: 0x%02X -> 0x%02X\n", currentErrors, newErrors);
    }

    currentErrors = newErrors;
}

void handleErrors() {
    checkForErrors();

    if (currentErrors != 0 && currentState != ERROR) {
        Serial.println("Errors detected, transitioning to ERROR state");
        transitionTo(ERROR, handleErrorState);
    }
}

// ----------------- State Handlers -----------------

void handleIdleState() {
    if (waterSensor.getLevel() && waterSensor.getLevel() < 10 && !laserSensor.isHopperLow()) {
        Serial.printf("Water level %d%% below threshold, transitioning to MIXING\n", waterSensor.getLevel());
        transitionTo(MIXING, handleMixingState);
    }
}

void handleMixingState() {
    int currentWaterLevel = waterSensor.getLevel();
    
    if (currentWaterLevel > 90) {
        Serial.printf("Water level %d%% above threshold, transitioning to WAITING_POST_MIX\n", currentWaterLevel);
        sensorActivatedDuringPostMix = false;
        transitionTo(WAITING_POST_MIX, handleWaitingPostMixState);
    }
}

void handleWaitingPostMixState() {
    int currentWaterLevel = waterSensor.getLevel();
    
    if (currentWaterLevel < 10) {
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
    clearLCD();
    setCursor(0, 0);
    printLCD("ERROR:");

    if (currentErrors & ERROR_HOPPER_LOW) {
        printLCD("Low Hopper");
    }
    if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
        printLCD("Mix Time Exceeded");
    }
    if (currentErrors & ERROR_WATER_SENSOR_FAILURE) {
        printLCD("Water Sensor Fail");
    }

    checkForErrors();

    if (currentErrors == 0) {
        Serial.println("Errors cleared, returning to IDLE");
        transitionTo(IDLE, handleIdleState);
    }
}

// ----------------- Debug Information -----------------

void printDebugInfo() {
    debugPrintln("\n=== System Status ===");

    // Smaller buffer size to reduce memory usage
    char buffer[60];

    // Current state and uptime
    snprintf(buffer, sizeof(buffer), "State: %s, Uptime: %lus", 
             getStateString(currentState), millis() / 1000);
    debugPrintln(buffer);

    // Water and hopper levels
    snprintf(buffer, sizeof(buffer), "Water Level: %d%%, Hopper Level: %d%%", 
             waterSensor.getLevel(), hopperLevel);
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
