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
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>  // For Telnet
#include <ESPmDNS.h>     // For mDNS service

// State function pointer
typedef void (*StateHandler)();
StateHandler currentStateHandler = nullptr;

// Global variables
State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSensorUpdate = 0;
int hopperLevel = 0;
bool sensorActivatedDuringPostMix = false;
uint8_t currentErrors = 0;
const uint8_t ERROR_HOPPER_LOW = 0x01;
const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02;
LaserSensor laserSensor;  // Laser sensor object
WaterLevelSensor waterSensor;

// Web Server
WebServer server(80);  // Initialize the web server on port 80


// Telnet Server
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// Error queue
#define MAX_ERRORS 5
uint8_t errorQueue[MAX_ERRORS];
int errorQueueStart = 0;
int errorQueueEnd = 0;

// Function declarations
void setupPins();
void updateSensors();
void handleErrors();
bool checkForErrors();
void transitionTo(State newState, StateHandler newStateHandler);
void executeCurrentState();
void enqueueError(uint8_t error);
uint8_t dequeueError();
bool isErrorQueueEmpty();
void handleIdleState();
void handleMixingState();
void handleWaitingPostMixState();
void handleWashStandbyState();
void handleWashDispenseState();
void handleErrorState();
State readWashMode();
void updateDevices(State state);
void setupOTA();
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

    waterSensor.begin();
    setupWiFi();  // Ensure this function connects to your WiFi network

    // Wait for WiFi connection before proceeding
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Start mDNS service
    if (!MDNS.begin("MyESP32")) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS responder started");
    }

    // Start the web server and ElegantOTA
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/plain", "Welcome to the OTA Update Server");
    });

    // Initialize ElegantOTA
    ElegantOTA.begin(&server);

    // Start the web server
    server.begin();
    Serial.println("HTTP server and ElegantOTA started");

    setupHomeAssistant();
    telnetServer.begin();  // Initialize Telnet server
    telnetServer.setNoDelay(true);  // Disable buffering for Telnet

    // Initial state
    transitionTo(IDLE, handleIdleState);
    displayMessage("System Ready");
    delay(1000);
}

void loop() {
    // Periodically update sensors
    if (millis() - lastSensorUpdate >= 1000) {
        updateSensors();
        lastSensorUpdate = millis();
    }

    // Process errors
    handleErrors();

    // Execute current state
    executeCurrentState();

    // Periodically update the LCD
    if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD(currentState, hopperLevel);
        lastLCDUpdate = millis();
    }

    loopHomeAssistant();

     // Handle web server requests and ElegantOTA updates
    server.handleClient();
    ElegantOTA.loop();

    handleTelnet();       // Handle Telnet communication
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
    DEBUG_PRINTF("Transitioning to state: %s\n", getStateString(newState));
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);  // Ensure devices are updated on state change
}

// ----------------- Sensor and Error Management -----------------

void updateSensors() {
    hopperLevel = laserSensor.readHopperLevel();
    waterSensor.update();
}

void handleErrors() {
    if (!checkForErrors()) {
        return;
    }

    // Process the first error in the queue
    if (!isErrorQueueEmpty()) {
        currentErrors = dequeueError();
        transitionTo(ERROR, handleErrorState);
    }
}

bool checkForErrors() {
    uint8_t newErrors = 0;

    if (laserSensor.isHopperLow()) {
        enqueueError(ERROR_HOPPER_LOW);
        newErrors |= ERROR_HOPPER_LOW;
    }

    if (currentState == MIXING && (millis() - stateStartTime >= maxMixingDuration)) {
        enqueueError(ERROR_MIX_TIME_EXCEEDED);
        newErrors |= ERROR_MIX_TIME_EXCEEDED;
    }

    return newErrors != 0;
}

void enqueueError(uint8_t error) {
    errorQueue[errorQueueEnd] = error;
    errorQueueEnd = (errorQueueEnd + 1) % MAX_ERRORS;
}

uint8_t dequeueError() {
    uint8_t error = errorQueue[errorQueueStart];
    errorQueueStart = (errorQueueStart + 1) % MAX_ERRORS;
    return error;
}

bool isErrorQueueEmpty() {
    return errorQueueStart == errorQueueEnd;
}

// ----------------- State Handlers -----------------

void handleIdleState() {
    if (waterSensor.isReliable() && waterSensor.getLevel() < 10 && !laserSensor.isHopperLow()) {
        transitionTo(MIXING, handleMixingState);
    } else if (laserSensor.isHopperLow()) {
        enqueueError(ERROR_HOPPER_LOW);
    }
}

void handleMixingState() {
    if (waterSensor.getLevel() > 90) {
        sensorActivatedDuringPostMix = false;
        transitionTo(WAITING_POST_MIX, handleWaitingPostMixState);
    } else if (laserSensor.isHopperLow()) {
        enqueueError(ERROR_HOPPER_LOW);
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
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
}

void handleWashDispenseState() {
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, LOW);  // Water on
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
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

    // Check if the error can be cleared
    if (isErrorQueueEmpty()) {
        currentErrors = 0;
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
            digitalWrite(waterPin, LOW);  // Water on
            digitalWrite(augerPin, HIGH);
            digitalWrite(agitatorPin, HIGH);
            break;
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
