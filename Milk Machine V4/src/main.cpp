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
    EMPTY_HOPPER_ERROR = 4
};

// System states
State currentState = IDLE;  // Initial state
ErrorCode currentError = NO_ERROR;

// Timing Variables
unsigned long postMixingStart = 0;
bool timeoutOccurred = false;                       // Flag to indicate timeout
bool errorMessageDisplayed = false;                 // Flag to prevent multiple error messages
unsigned long mixingStart = 0;                      // Start time for mixing state
unsigned long lastCurrentCheck = 0;                 // Last time current was checked
unsigned long idleStart = 0;                        // Start time for idle state
unsigned long lastStatsUpdate = 0;                  // Last time stats were updated to HA

// Debounce Variables
unsigned long lastLevelChangeTime = 0;    // Last time the level state changed
bool debouncedLevelState = false;         // Debounced level state

// Create instances of our managers
LCDManager lcdManager(0x2D);  // Pass the correct I2C address
SystemMonitor sysMonitor;
MotorMonitor motorMonitor(AUGER_CURRENT_PIN, MIXER_CURRENT_PIN);

// Function Declarations
void idleState();
void mixingState();
void postMixingState();
void washState();
void errorState();
void turnAllRelays(uint8_t state);
void updateState(State newState);
void mixingtimout();
bool isLevelReached();
void updateDisplay();
void logSystemError(ErrorCode error, const char* message);
void checkMotorCurrents();

// *** Main Setup Function ***

void setup() {
    // Initialize Serial communication
    Serial.begin(9600);

    // Initialize system monitor
    sysMonitor.begin();

    // Initialize motor monitor
    motorMonitor.begin();

    // Wifi setup
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    WiFi.setHostname(HOSTNAME);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi.");

    // Initialize Home Assistant integration
    setupHomeAssistant();

    // Initialize the LCD
    lcdManager.begin();
    lcdManager.updateDisplay("IDLE");  // Initialize display with IDLE state
    Serial.println("LCD initialized.");

    // Set relay pins as outputs
    pinMode(MIXER_PIN, OUTPUT);
    pinMode(WATER_PIN, OUTPUT);
    pinMode(AUGER_PIN, OUTPUT);
    pinMode(AGITATOR_PIN, OUTPUT);

    // Set switch pins as inputs with pull-up resistors
    pinMode(WASH_STANDBY_PIN, INPUT_PULLUP);
    pinMode(WASH_DISPENSE_PIN, INPUT_PULLUP);
    pinMode(LIQUID_LEVEL_PIN, INPUT_PULLUP);

    // Initialize all relays to OFF state
    turnAllRelays(RELAY_OFF);

    // Initial debug output
    Serial.println("System Initialized. Starting in IDLE state.");

    // Initialize debounce variables
    debouncedLevelState = false;
    lastLevelChangeTime = millis();

    // Set initial idle start time
    idleStart = millis();

    // Calibrate motor current baselines if enabled
    if (ENABLE_CURRENT_SENSING) {
        delay(1000);  // Wait for power to stabilize
        motorMonitor.calibrateBaseline();
    }
}

// *** Main Loop Function ***

void loop() {
    // Feed the watchdog
    sysMonitor.feedWatchdog();

    // Update system statistics
    sysMonitor.updateStats();

    // Update Home Assistant stats every minute
    if (millis() - lastStatsUpdate >= 60000) {  // Every minute
        updateHomeAssistantStats(
            sysMonitor.getTotalMixes(),
            sysMonitor.getTotalRuntime(),
            sysMonitor.getErrorCount(),
            sysMonitor.getCurrentUptime()
        );
        lastStatsUpdate = millis();
    }

    // Update motor current readings if enabled
    if (ENABLE_CURRENT_SENSING) {
        motorMonitor.update();

        // Check motor currents periodically
        if (millis() - lastCurrentCheck >= CURRENT_CHECK_INTERVAL) {
            checkMotorCurrents();
            lastCurrentCheck = millis();
        }
    }

    // Handle Home Assistant MQTT connection
    loopHomeAssistant();

    // Check wash standby switch and handle state transitions
    if (digitalRead(WASH_STANDBY_PIN) == SWITCH_ON && currentState != WASH) {
        updateState(WASH);
    }

    // Update current state based on liquid level and wash switches
    switch (currentState) {
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
        if (millis() - idleStart >= MIX_INTERVAL) {
            digitalWrite(MIXER_PIN, RELAY_ON);
            mixerStartTime = millis();
            mixerRunning = true;
        }
    } else {
        // Check if mixing duration is complete
        if (millis() - mixerStartTime >= IDLE_MIX_DURATION) {
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
    if ((millis() - mixingStart) >= MIXING_TIMEOUT) {
        timeoutOccurred = true;
        currentError = TIMEOUT_ERROR;
        logSystemError(TIMEOUT_ERROR, ERROR_MSG_TIMEOUT);
        updateState(ERROR);
        initializeTimer = true;
        return;
    }

    turnAllRelays(RELAY_ON);

    // Check for empty hopper only if current sensing is enabled
    if (ENABLE_CURRENT_SENSING && motorMonitor.isHopperEmpty()) {
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

    if (millis() - postMixingStart >= POST_MIXING_DURATION) {
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
            default:
                errorMsg = ERROR_MSG_UNKNOWN;
                break;
        }
        
        lcdManager.showError(errorMsg);
        errorMessageDisplayed = true;

        // Create error message for Home Assistant
        char fullMessage[100];
        snprintf(fullMessage, sizeof(fullMessage), "ERROR: %s", errorMsg);
        updateHomeAssistant(fullMessage);
    }

    // Wait for wash switch toggle to clear error
    if (digitalRead(WASH_STANDBY_PIN) == SWITCH_ON) {
        timeoutOccurred = false;
        errorMessageDisplayed = false;
        currentError = NO_ERROR;
        updateState(IDLE);
    }
}

// *** Helper Functions ***

void checkMotorCurrents() {
    // Only check currents when motors are running and current sensing is enabled
    if (ENABLE_CURRENT_SENSING && (currentState == MIXING || currentState == POST_MIXING)) {
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
        case IDLE: stateStr = "IDLE"; break;
        case MIXING: stateStr = "MIXING"; break;
        case POST_MIXING: stateStr = "POST-MIX"; break;
        case WASH: stateStr = "WASH"; break;
        case ERROR: stateStr = "ERROR"; break;
        default: stateStr = "UNKNOWN"; break;
    }
    
    // Update display with state
    lcdManager.updateDisplay(stateStr);

    // Update Home Assistant with state
    updateHomeAssistant(stateStr);
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

        // Update Home Assistant
        updateHomeAssistant(stateStr);
        
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

    if ((millis() - lastLevelChangeTime) >= DEBOUNCE_DELAY) {
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
