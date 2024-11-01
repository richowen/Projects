#include <Arduino.h>
#include "DFRobot_RGBLCD1602.h"
#include "home_assistant.h"
#include "hopper_sensor.h"
#include <WiFi.h>
#include "lcd_manager.h"

// *** Constants and Global Variables ***

// Pin Definitions
const int mixerPin = 16;       // Mixer motor relay
const int waterPin = 17;       // Water dispenser relay
const int augerPin = 25;       // Powder auger relay
const int agitatorPin = 26;    // Powder agitator relay
const int washStandbyPin = 23; // Wash standby switch
const int washDispensePin = 5; // Water solenoid activate switch in wash mode
const int liquidLevelPin = 12; // Liquid level pressure switch

// Relay States
const uint8_t RELAY_ON = LOW; // Active LOW relays
const uint8_t RELAY_OFF = HIGH;

// Reversed Switch States
const uint8_t SWITCH_ON = HIGH; // Switch connected to VCC when ON (open)
const uint8_t SWITCH_OFF = LOW; // Pulled LOW when OFF (closed)

// State Definitions
enum State
{
    IDLE,
    MIXING,
    POST_MIXING,
    WASH,
    ERROR
}; 

// System states
State currentState = IDLE; // Initial state

enum ErrorType
{
    NO_ERROR,
    TIMEOUT_ERROR,
    HOPPER_LOW_ERROR
};
ErrorType currentError = NO_ERROR;

// Timing Variables
unsigned long postMixingStart = 0;
const unsigned long postMixingDuration = 5000;     // 5 seconds
unsigned long mixingStart = 0;                     // Start time for mixing state
const unsigned long mixingTimeout = 60000;         // 60 seconds
bool timeoutOccurred = false;                      // Flag to indicate timeout
bool errorMessageDisplayed = false;                // Flag to prevent multiple error messages
unsigned long lastHopperUpdate = 0;                // Last time hopper level was updated
const unsigned long HOPPER_UPDATE_INTERVAL = 1000; // 1 second in milliseconds
unsigned long idleStart = 0;                       // Start time for idle state
const unsigned long MIX_INTERVAL = 300000;         // 5 minutes of idle time
const unsigned long IDLE_MIX_DURATION = 5000;      // 5 seconds of mixing

// Debounce Variables
const unsigned long debounceDelay = 500; // Aggressive debounce delay in milliseconds
unsigned long lastLevelChangeTime = 0;   // Last time the level state changed
bool debouncedLevelState = false;        // Debounced level state

// LCD Configuration
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2); // I2C address 0x2D, 16x2 display

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
void updateHopperLevel();

// Create LCDManager instance
LCDManager lcdManager;

// Create hopper sensor instance
HopperSensor hopperSensor;

// *** Main Setup Function ***

void setup()
{
    // Initialize Serial communication
    Serial.begin(9600);

    // Wifi setup
    WiFi.begin("WiFi", "Gliders1!");
    IPAddress staticIP(192, 168, 1, 5);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.setHostname("Milk_Machine");
    WiFi.config(staticIP, gateway, subnet);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi.");

    // Initialize Home Assistant integration
    setupHomeAssistant();

    // Initialize the LCD
    lcdManager.begin();
    lcdManager.updateDisplay("IDLE", 0);
    Serial.println("LCD initialized.");

    // Initialize hopper sensor
    hopperSensor.begin();

    // Set relay pins as outputs
    pinMode(mixerPin, OUTPUT);
    pinMode(waterPin, OUTPUT);
    pinMode(augerPin, OUTPUT);
    pinMode(agitatorPin, OUTPUT);

    // Set switch pins as inputs with pull-up resistors
    pinMode(washStandbyPin, INPUT_PULLUP);
    pinMode(washDispensePin, INPUT_PULLUP);
    pinMode(liquidLevelPin, INPUT_PULLUP);

    // Initialize all relays to OFF state
    turnAllRelays(RELAY_OFF);

    // Initial debug output
    Serial.println("System Initialized. Starting in IDLE state.");

    // Display initial state on LCD
    lcd.setCursor(0, 1);
    lcd.print("State: IDLE     ");

    // Initialize debounce variables
    debouncedLevelState = false;
    lastLevelChangeTime = millis();

    // Set initial idle start time
    idleStart = millis();
}

// *** Main Loop Function ***

// Main Loop
void loop()
{
    // Handle Home Assistant MQTT connection
    loopHomeAssistant();

    // Check wash standby switch and handle state transitions
    if (digitalRead(washStandbyPin) == SWITCH_ON && currentState != WASH)
    {
        updateState(WASH);
    }

    // Update current state based on liquid level and wash switches
    switch (currentState)
    {
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

    // Update hopper level readings and display
    updateHopperLevel();
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
            digitalWrite(mixerPin, RELAY_ON);
            mixerStartTime = millis();
            mixerRunning = true;
        }
    } else {
        // Check if mixing duration is complete
        if (millis() - mixerStartTime >= IDLE_MIX_DURATION) {
            digitalWrite(mixerPin, RELAY_OFF);
            mixerRunning = false;
            idleStart = millis();  // Reset the interval timer
        }
    }
}

void mixingState()
{
    static bool initializeTimer = true;
    if (initializeTimer)
    {
        mixingStart = millis();
        initializeTimer = false;
        timeoutOccurred = false;
    }

    // Check for timeout
    if ((millis() - mixingStart) >= mixingTimeout)
    {
        timeoutOccurred = true;
        currentError = TIMEOUT_ERROR;
        updateState(ERROR);
        initializeTimer = true;
        return;
    }

    turnAllRelays(RELAY_ON);
    if (isLevelReached())
    {
        updateState(POST_MIXING);
        postMixingStart = millis();
        initializeTimer = true; // Reset for next time
    }
}

void postMixingState()
{
    // First check liquid level - highest priority
    if (!isLevelReached())
    {
        updateState(MIXING);
        return; // Exit immediately to handle the low level
    }

    // If level is OK, continue with normal post-mixing behavior
    digitalWrite(mixerPin, RELAY_ON);
    digitalWrite(waterPin, RELAY_OFF);
    digitalWrite(augerPin, RELAY_OFF);
    digitalWrite(agitatorPin, RELAY_OFF);

    if (millis() - postMixingStart >= postMixingDuration)
    {
        updateState(IDLE);
    }
}

void washState()
{
    turnAllRelays(RELAY_OFF);

    if (digitalRead(washDispensePin) == SWITCH_ON)
    {
        digitalWrite(waterPin, RELAY_ON); // Dispense water
    }
    else
    {
        digitalWrite(waterPin, RELAY_OFF); // Turn off water dispenser
    }

    if (digitalRead(washStandbyPin) == SWITCH_OFF)
    {
        updateState(IDLE);
    }
}

void errorState()
{
    turnAllRelays(RELAY_OFF);

    // Display appropriate error message if not already displayed
    if (!errorMessageDisplayed)
    {
        switch (currentError)
        {
        case TIMEOUT_ERROR:
            lcdManager.showError("Mixing Timeout");
            break;
        case HOPPER_LOW_ERROR:
            lcdManager.showError("Hopper Low!");
            break;
        default:
            lcdManager.showError("Unknown Error");
            break;
        }
        errorMessageDisplayed = true;

        // Create error message for Home Assistant
        char fullMessage[100];
        snprintf(fullMessage, sizeof(fullMessage), "ERROR: %s", 
            currentError == TIMEOUT_ERROR ? "Mixing Timeout" :
            currentError == HOPPER_LOW_ERROR ? "Hopper Low!" : "Unknown Error");
        updateHomeAssistant(fullMessage);
    }

    // Wait for wash switch toggle to clear error
    if (digitalRead(washStandbyPin) == SWITCH_ON)
    {
        timeoutOccurred = false;
        errorMessageDisplayed = false;
        currentError = NO_ERROR;
        updateState(IDLE);
    }
}

// *** Helper Functions ***

void updateHopperLevel() {
    if (millis() - lastHopperUpdate >= HOPPER_UPDATE_INTERVAL) {
        int hopperPercent = hopperSensor.getPercentage();
        char hopperLevel[8];
        snprintf(hopperLevel, sizeof(hopperLevel), "%d", hopperPercent);
        mqttClient.publish(MQTT_HOPPER_TOPIC, hopperLevel);
        lastHopperUpdate = millis();

        // Update LCD with current state and hopper level
        const char* stateStr;
        switch (currentState) {
            case IDLE: stateStr = "IDLE"; break;
            case MIXING: stateStr = "MIXING"; break;
            case POST_MIXING: stateStr = "POST-MIX"; break;
            case WASH: stateStr = "WASH"; break;
            case ERROR: stateStr = "ERROR"; break;
            default: stateStr = "UNKNOWN"; break;
        }
        lcdManager.updateDisplay(stateStr, hopperPercent);

        // Check hopper level
        if (hopperSensor.isLow() && currentState != WASH && currentState != ERROR) {
            currentError = HOPPER_LOW_ERROR;
            lcdManager.showError("Hopper Low!");
            updateState(ERROR);
        }
    }
}
// Relay Control Functions
void turnAllRelays(uint8_t state)
{
    digitalWrite(mixerPin, state);
    digitalWrite(waterPin, state);
    digitalWrite(augerPin, state);
    digitalWrite(agitatorPin, state);
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
        
        // The regular display update will happen in updateHopperLevel()
        Serial.print("State changed to: ");
        Serial.println(displayStr);
    }
}


// Liquid Level Function with Aggressive Debouncing
bool isLevelReached()
{
    static bool lastRawLevelState = SWITCH_OFF;
    bool currentRawLevelState = digitalRead(liquidLevelPin);

    if (currentRawLevelState != lastRawLevelState)
    {
        lastLevelChangeTime = millis(); // Reset the debounce timer
    }

    if ((millis() - lastLevelChangeTime) >= debounceDelay)
    {
        if (currentRawLevelState != debouncedLevelState)
        {
            debouncedLevelState = currentRawLevelState;
        }
    }

    lastRawLevelState = currentRawLevelState;
    return debouncedLevelState == SWITCH_ON;
}

void displayErrorMessage(const char* errorMessage) {
    if (!errorMessageDisplayed) {
        lcdManager.showError(errorMessage);
        errorMessageDisplayed = true;

        // Create error message for Home Assistant
        char fullMessage[100];
        snprintf(fullMessage, sizeof(fullMessage), "ERROR: %s", errorMessage);
        updateHomeAssistant(fullMessage);
    }
}
