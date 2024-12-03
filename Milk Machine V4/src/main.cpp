#include <Arduino.h>
#include "DFRobot_RGBLCD1602.h"
#include "home_assistant.h"
#include <WiFi.h>
#include "lcd_manager.h"
#include "system_monitor.h"
#include "motor_monitor.h"
#include "config.h"

// State Definitions
enum State {
    INIT,           // Added initialization state
    IDLE,
    MIXING,
    POST_MIXING,
    WASH,
    ERROR
}; 

// Error Codes
enum ErrorCode {
    NO_ERROR = 0,
    TIMEOUT_ERROR = 1,
    MOTOR_CURRENT_ERROR = 2,
    WATER_PRESSURE_ERROR = 3,
    EMPTY_HOPPER_ERROR = 4,
    CALIBRATION_ERROR = 5  // Added calibration error
};

// System states
State currentState = INIT;  // Start in INIT state
ErrorCode currentError = NO_ERROR;

// Timing Variables
unsigned long postMixingStart = 0;
bool timeoutOccurred = false;
bool errorMessageDisplayed = false;
unsigned long mixingStart = 0;
unsigned long lastCurrentCheck = 0;
unsigned long idleStart = 0;

// Debounce Variables
unsigned long lastLevelChangeTime = 0;
bool debouncedLevelState = false;

// Create instances of our managers
LCDManager lcdManager;
SystemMonitor sysMonitor;
MotorMonitor motorMonitor(AUGER_CURRENT_PIN, MIXER_CURRENT_PIN);

// Function Declarations
void idleState();
void mixingState();
void postMixingState();
void washState();
void errorState();
void initState();
void turnAllRelays(uint8_t state);
void updateState(State newState);
bool isLevelReached();
void updateDisplay();
void logSystemError(ErrorCode error, const char* message);
void checkMotorCurrents();

// Helper function to handle millis() overflow
unsigned long getElapsedTime(unsigned long start) {
    return (long)(millis() - start);
}

// Power-on state determination
State determineInitialState() {
    // Check if we're recovering from an error
    if (sysMonitor.getLastError() != NO_ERROR) {
        return ERROR;
    }
    
    // Check liquid level to determine if mixing needed
    if (!isLevelReached()) {
        return MIXING;
    }
    
    return IDLE;
}

// *** Main Setup Function ***

void setup() {
    // Initialize Serial communication
    Serial.begin(9600);

    // Initialize system monitor first for error logging
    if (!sysMonitor.begin()) {
        Serial.println("Failed to initialize system monitor!");
        while(1) { delay(1000); }  // Halt if critical initialization fails
    }

    // Set pin modes before anything else
    pinMode(MIXER_PIN, OUTPUT);
    pinMode(WATER_PIN, OUTPUT);
    pinMode(AUGER_PIN, OUTPUT);
    pinMode(AGITATOR_PIN, OUTPUT);
    pinMode(WASH_STANDBY_PIN, INPUT_PULLUP);
    pinMode(WASH_DISPENSE_PIN, INPUT_PULLUP);
    pinMode(LIQUID_LEVEL_PIN, INPUT_PULLUP);

    // Initialize all relays to OFF state
    turnAllRelays(RELAY_OFF);

    // Initialize LCD with retry mechanism
    for (int retry = 0; retry < 3; retry++) {
        lcdManager.begin();
        delay(100);  // Give LCD time to initialize
        lcdManager.tryDisplay("Testing LCD", "Please Wait...");
        if (lcdManager.isInitialized()) {
            break;
        }
    }
    
    if (!lcdManager.isInitialized()) {
        Serial.println("Failed to initialize LCD!");
        logSystemError(CALIBRATION_ERROR, "LCD initialization failed");
        while(1) { delay(1000); }  // Halt if LCD fails
    }

    // Initialize motor monitor
    if (!motorMonitor.begin()) {
        logSystemError(CALIBRATION_ERROR, "Motor monitor initialization failed");
        lcdManager.showError("Motor Init Fail");
        while(1) { delay(1000); }  // Halt if motor monitor fails
    }

    // Initialize WiFi with timeout
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    WiFi.setHostname(HOSTNAME);
    
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (getElapsedTime(wifiStart) > 30000) { // 30 second timeout
            Serial.println("WiFi connection timeout");
            break;  // Continue without WiFi
        }
        delay(500);
        Serial.println("Connecting to WiFi...");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi.");
        setupHomeAssistant();
    }

    // Initialize debounce variables
    debouncedLevelState = false;
    lastLevelChangeTime = millis();

    // Set initial idle start time
    idleStart = millis();

    // Determine and set initial state
    updateState(determineInitialState());
    
    Serial.println("System Initialized.");
}

// *** Main Loop Function ***

void loop() {
    // Feed the watchdog
    sysMonitor.feedWatchdog();

    // Update system statistics
    sysMonitor.updateStats();

    // Update motor current readings
    motorMonitor.update();

    // Check motor currents periodically
    if (getElapsedTime(lastCurrentCheck) >= CURRENT_CHECK_INTERVAL) {
        checkMotorCurrents();
        lastCurrentCheck = millis();
    }

    // Handle Home Assistant MQTT connection if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        loopHomeAssistant();
    }

    // Check wash standby switch and handle state transitions
    if (digitalRead(WASH_STANDBY_PIN) == SWITCH_ON && currentState != WASH) {
        updateState(WASH);
    }

    // Update current state based on liquid level and wash switches
    switch (currentState) {
        case INIT:
            initState();
            break;
        case IDLE:
            idleState();
            break;
        case MIXING:
            mixingState();
            break;
        case POST_MIXING:
            postMixingState();
            break;
        case WASH:
            washState();
            break;
        case ERROR:
            errorState();
            break;
    }

    // Update display
    updateDisplay();
}

// *** State Functions ***

void initState() {
    // Perform any initialization tasks
    turnAllRelays(RELAY_OFF);
    
    // Calibrate motor current baselines
    if (motorMonitor.calibrateBaseline()) {
        updateState(determineInitialState());
    } else {
        logSystemError(CALIBRATION_ERROR, "Motor calibration failed");
        updateState(ERROR);
    }
}

void idleState() {
    static bool mixerRunning = false;
    static unsigned long mixerStartTime = 0;

    // Default state - all relays off
    if (!mixerRunning) {
        turnAllRelays(RELAY_OFF);
    }

    // Check if liquid level is low - highest priority
    if (!isLevelReached()) {
        mixerRunning = false;  // Reset mixer state
        updateState(MIXING);
        return;
    }

    // Handle periodic mixing
    if (!mixerRunning) {
        // Start a mixing cycle if interval has elapsed
        if (getElapsedTime(idleStart) >= MIX_INTERVAL) {
            digitalWrite(MIXER_PIN, RELAY_ON);
            mixerStartTime = millis();
            mixerRunning = true;
        }
    } else {
        // Check if mixing duration is complete
        if (getElapsedTime(mixerStartTime) >= IDLE_MIX_DURATION) {
            digitalWrite(MIXER_PIN, RELAY_OFF);
            mixerRunning = false;
            idleStart = millis();  // Reset the interval timer
        }
    }
}

void mixingState() {
    static bool initializeTimer = true;
    if (initializeTimer) {
        mixingStart = millis();
        initializeTimer = false;
        timeoutOccurred = false;
    }

    // Check for timeout
    if (getElapsedTime(mixingStart) >= MIXING_TIMEOUT) {
        timeoutOccurred = true;
        currentError = TIMEOUT_ERROR;
        logSystemError(TIMEOUT_ERROR, ERROR_MSG_TIMEOUT);
        updateState(ERROR);
        initializeTimer = true;
        return;
    }

    turnAllRelays(RELAY_ON);

    // Check for empty hopper
    if (motorMonitor.isHopperEmpty()) {
        currentError = EMPTY_HOPPER_ERROR;
        logSystemError(EMPTY_HOPPER_ERROR, ERROR_MSG_HOPPER);
        updateState(ERROR);
        return;
    }

    if (isLevelReached()) {
        sysMonitor.recordMix();  // Record successful mix
        updateState(POST_MIXING);
        postMixingStart = millis();
        initializeTimer = true;  // Reset for next time
    }
}

void postMixingState() {
    // First check liquid level - highest priority
    if (!isLevelReached()) {
        updateState(MIXING);
        return;  // Exit immediately to handle the low level
    }

    // If level is OK, continue with normal post-mixing behavior
    digitalWrite(MIXER_PIN, RELAY_ON);
    digitalWrite(WATER_PIN, RELAY_OFF);
    digitalWrite(AUGER_PIN, RELAY_OFF);
    digitalWrite(AGITATOR_PIN, RELAY_OFF);

    if (getElapsedTime(postMixingStart) >= POST_MIXING_DURATION) {
        updateState(IDLE);
    }
}

void washState() {
    turnAllRelays(RELAY_OFF);

    if (digitalRead(WASH_DISPENSE_PIN) == SWITCH_ON) {
        digitalWrite(WATER_PIN, RELAY_ON);  // Dispense water
    } else {
        digitalWrite(WATER_PIN, RELAY_OFF);  // Turn off water dispenser
    }

    if (digitalRead(WASH_STANDBY_PIN) == SWITCH_OFF) {
        updateState(IDLE);
    }
}

void errorState() {
    turnAllRelays(RELAY_OFF);

    // Display appropriate error message if not already displayed
    if (!errorMessageDisplayed) {
        const char* errorMsg;
        switch (currentError) {
            case TIMEOUT_ERROR:
                errorMsg = ERROR_MSG_TIMEOUT;
                break;
            case MOTOR_CURRENT_ERROR:
                errorMsg = ERROR_MSG_MOTOR;
                break;
            case WATER_PRESSURE_ERROR:
                errorMsg = ERROR_MSG_WATER;
                break;
            case EMPTY_HOPPER_ERROR:
                errorMsg = ERROR_MSG_HOPPER;
                break;
            case CALIBRATION_ERROR:
                errorMsg = "Calibration Err";
                break;
            default:
                errorMsg = ERROR_MSG_UNKNOWN;
                break;
        }
        
        lcdManager.showError(errorMsg);
        errorMessageDisplayed = true;

        // Create error message for Home Assistant
        char fullMessage[100];
        snprintf(fullMessage, sizeof(fullMessage), "ERROR: %s", errorMsg);
        if (WiFi.status() == WL_CONNECTED) {
            updateHomeAssistant(fullMessage);
        }
    }

    // Wait for wash switch toggle to clear error
    if (digitalRead(WASH_STANDBY_PIN) == SWITCH_ON) {
        timeoutOccurred = false;
        errorMessageDisplayed = false;
        currentError = NO_ERROR;
        updateState(INIT);  // Go through initialization again
    }
}

// *** Helper Functions ***

void checkMotorCurrents() {
    // Only check currents when motors are running
    if (currentState == MIXING || currentState == POST_MIXING) {
        // Check for mixer motor overload
        if (motorMonitor.isMixerOverload()) {
            currentError = MOTOR_CURRENT_ERROR;
            logSystemError(MOTOR_CURRENT_ERROR, ERROR_MSG_MOTOR);
            updateState(ERROR);
            return;
        }

        // Log current values for diagnostics
        Serial.print("Currents - Auger: ");
        Serial.print(motorMonitor.getAugerCurrent());
        Serial.print("A, Mixer: ");
        Serial.print(motorMonitor.getMixerCurrent());
        Serial.println("A");
    }
}

void updateDisplay() {
    // Update LCD with current state
    const char* stateStr;
    switch (currentState) {
        case INIT: stateStr = "INIT"; break;
        case IDLE: stateStr = "IDLE"; break;
        case MIXING: stateStr = "MIXING"; break;
        case POST_MIXING: stateStr = "POST-MIX"; break;
        case WASH: stateStr = "WASH"; break;
        case ERROR: stateStr = "ERROR"; break;
        default: stateStr = "UNKNOWN"; break;
    }
    
    // Update display with state only
    lcdManager.updateDisplay(stateStr, -1);

    // Update Home Assistant if connected
    if (WiFi.status() == WL_CONNECTED) {
        updateHomeAssistant(stateStr);
    }
}

// Relay Control Functions
void turnAllRelays(uint8_t state) {
    digitalWrite(MIXER_PIN, state);
    digitalWrite(WATER_PIN, state);
    digitalWrite(AUGER_PIN, state);
    digitalWrite(AGITATOR_PIN, state);
}

// Function to update the current state and print state change if necessary
void updateState(State newState) {
    if (newState != currentState) {
        currentState = newState;
        errorMessageDisplayed = false;

        // Convert state to string for Home Assistant
        const char* stateStr;
        const char* displayStr;
        switch (currentState) {
            case INIT:
                stateStr = "init";
                displayStr = "INIT";
                break;
            case IDLE:
                stateStr = "idle";
                displayStr = "IDLE";
                break;
            case MIXING:
                stateStr = "mixing";
                displayStr = "MIXING";
                break;
            case POST_MIXING:
                stateStr = "post_mixing";
                displayStr = "POST-MIX";
                break;
            case WASH:
                stateStr = "wash";
                displayStr = "WASH";
                break;
            case ERROR:
                stateStr = "error";
                displayStr = "ERROR";
                break;
            default:
                stateStr = "unknown";
                displayStr = "UNKNOWN";
                break;
        }

        // Update Home Assistant if connected
        if (WiFi.status() == WL_CONNECTED) {
            updateHomeAssistant(stateStr);
        }
        
        // Update LCD color and state
        lcdManager.setStateColor(displayStr);
        
        Serial.print("State changed to: ");
        Serial.println(displayStr);
    }
}

// Liquid Level Function with Aggressive Debouncing
bool isLevelReached() {
    static bool lastRawLevelState = SWITCH_OFF;
    bool currentRawLevelState = digitalRead(LIQUID_LEVEL_PIN);

    if (currentRawLevelState != lastRawLevelState) {
        lastLevelChangeTime = millis();  // Reset the debounce timer
    }

    if (getElapsedTime(lastLevelChangeTime) >= DEBOUNCE_DELAY) {
        if (currentRawLevelState != debouncedLevelState) {
            debouncedLevelState = currentRawLevelState;
        }
    }

    lastRawLevelState = currentRawLevelState;
    return debouncedLevelState == SWITCH_ON;
}

void logSystemError(ErrorCode error, const char* message) {
    char fullMessage[100];
    snprintf(fullMessage, sizeof(fullMessage), "[%lu] %s", millis(), message);
    sysMonitor.logError(static_cast<uint8_t>(error), fullMessage);
}

