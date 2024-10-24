#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "laser_sensor.h"
#include "water_level_sensor.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <WiFiClient.h>  // For Telnet
#include <ESPmDNS.h>     // For mDNS service

// ----------------- Global Variables -----------------

// State function pointer
typedef void (*StateHandler)();
StateHandler currentStateHandler = nullptr;

// Error flags
const uint8_t ERROR_HOPPER_LOW = 0x01;
const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02;

State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSensorUpdate = 0;
int hopperLevel = 0;
bool sensorActivatedDuringPostMix = false;
uint8_t currentErrors = 0;

// Sensor objects
LaserSensor laserSensor;         // Laser sensor object
WaterLevelSensor waterSensor;    // Water level sensor object

// Web Server
WebServer server(80);  // Initialize WebServer

// Telnet Server
WiFiServer telnetServer(23);
WiFiClient telnetClient;

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

// ----------------- Setup and Loop -----------------

void setup() {
    Serial.begin(115200);
    Wire.begin();
    delay(100);  // Allow I2C to stabilize

    setupPins();
    setupLCD();

    // Initialize laser sensor
    if (!laserSensor.begin()) {
        Serial.println("Laser sensor initialization failed!");
    }

    // Initialize water level sensor
    waterSensor.begin();

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

    // Initialize ElegantOTA
    ElegantOTA.begin(&server);

    // Start the web server
    server.begin();
    Serial.println("HTTP server and ElegantOTA started");

    // Setup Home Assistant integration
    setupHomeAssistant();

    // Initialize Telnet server
    telnetServer.begin();
    telnetServer.setNoDelay(true);

    // Initial state
    transitionTo(IDLE, handleIdleState);
    displayMessage("System Ready");
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

    // Handle web server requests and ElegantOTA updates
    server.handleClient();

    // Handle Telnet communication
    handleTelnet();
}

// ----------------- Telnet Setup -----------------

void handleTelnet() {
    // Check if a new client has connected
    if (telnetServer.hasClient()) {
        if (!telnetClient || !telnetClient.connected()) {
            if (telnetClient) telnetClient.stop();  // Disconnect old client
            telnetClient = telnetServer.available();  // Accept new client
            Serial.println("New Telnet client connected");
        }
    }

    // Check if the client is still connected
    if (telnetClient && telnetClient.connected()) {
        while (telnetClient.available()) {
            char ch = telnetClient.read();
            Serial.write(ch);  // Echo data received from the client
        }
    }

    // Send serial output to Telnet client
    if (telnetClient && telnetClient.connected()) {
        if (Serial.available()) {
            char ch = Serial.read();
            telnetClient.write(ch);  // Forward serial data to Telnet client
        }
    }
}

// ----------------- State Management -----------------

void executeCurrentState() {
    if (currentStateHandler) {
        currentStateHandler();
    }
}

void transitionTo(State newState, StateHandler newStateHandler) {
    currentState = newState;
    currentStateHandler = newStateHandler;
    stateStartTime = millis();
    Serial.printf("Transitioning to state: %s\n", getStateString(newState));
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);  // Ensure devices are updated on state change
}

// ----------------- Sensor and Error Management -----------------

void updateSensors() {
    hopperLevel = laserSensor.readHopperLevel();
    waterSensor.update();
}

void checkForErrors() {
    uint8_t newErrors = 0;

    if (laserSensor.isHopperLow()) {
        newErrors |= ERROR_HOPPER_LOW;
    }

    if (currentState == MIXING && (millis() - stateStartTime >= maxMixingDuration)) {
        newErrors |= ERROR_MIX_TIME_EXCEEDED;
    }

    currentErrors = newErrors;
}

void handleErrors() {
    checkForErrors();

    if (currentErrors != 0 && currentState != ERROR) {
        transitionTo(ERROR, handleErrorState);
    }
}

// ----------------- State Handlers -----------------

void handleIdleState() {
    if (waterSensor.isReliable() && waterSensor.getLevel() < 10 && !laserSensor.isHopperLow()) {
        transitionTo(MIXING, handleMixingState);
    }
}

void handleMixingState() {
    if (waterSensor.getLevel() > 90) {
        sensorActivatedDuringPostMix = false;
        transitionTo(WAITING_POST_MIX, handleWaitingPostMixState);
    }
}

void handleWaitingPostMixState() {
    if (waterSensor.getLevel() < 10) {
        sensorActivatedDuringPostMix = true;
    }

    if (millis() - stateStartTime >= waitingDuration) {
        if (sensorActivatedDuringPostMix) {
            transitionTo(MIXING, handleMixingState);
        } else {
            transitionTo(IDLE, handleIdleState);
        }
    }
}

void handleWashStandbyState() {
    // Devices are already set in updateDevices()
    // You can add additional logic here if needed
}

void handleWashDispenseState() {
    // Devices are already set in updateDevices()
    // You can add additional logic here if needed
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

    // Re-check error conditions
    checkForErrors();

    if (currentErrors == 0) {
        // All errors resolved
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

    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
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
