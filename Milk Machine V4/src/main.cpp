#include <Arduino.h>
#include "DFRobot_RGBLCD1602.h"

// Pin Definitions
const int mixerPin = 16;         // Mixer motor relay
const int waterPin = 17;         // Water dispenser relay
const int augerPin = 25;         // Powder auger relay
const int agitatorPin = 26;      // Powder agitator relay
const int washStandbyPin = 23;   // Wash standby switch
const int washDispensePin = 5;   // Water solenoid activate switch in wash mode
const int liquidLevelPin = 12;   // Liquid level pressure switch

// Relay States
const uint8_t RELAY_ON = LOW;    // Active LOW relays
const uint8_t RELAY_OFF = HIGH;

// **Reversed Switch States**
const uint8_t SWITCH_ON = HIGH;   // Switch connected to VCC when ON (open)
const uint8_t SWITCH_OFF = LOW;   // Pulled LOW when OFF (closed)

// State Definitions
enum State { IDLE, MIXING, POST_MIXING, WASH, ERROR }; // System states
State currentState = IDLE; // Initial state

// Timing Variables
unsigned long postMixingStart = 0;
const unsigned long postMixingDuration = 5000; // 5 seconds
unsigned long mixingStart = 0; // Start time for mixing state
const unsigned long mixingTimeout = 10000; // 10 seconds
bool timeoutOccurred = false; // Flag to indicate timeout
bool errorMessageDisplayed = false;

// **Debounce Variables**
const unsigned long debounceDelay = 200; // Aggressive debounce delay in milliseconds
unsigned long lastLevelChangeTime = 0; // Last time the level state changed
bool debouncedLevelState = false; // Debounced level state

// LCD Configuration
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2);  // Adjusted I2C address to 0x2D

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

// LCD Helper Functions
void setLCDColor(int r, int g, int b);
void displayMessage(const char* message);
void displayErrorMessage(const char* errorMessage);

void setup() {
  // Initialize Serial communication
  Serial.begin(9600);

  // Initialize the LCD
  lcd.init();
  setLCDColor(0, 255, 0);  // Set initial backlight color to green
  lcd.clear();
  lcd.print("Milk Mixer Ready");
  Serial.println("LCD initialized.");

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
  lcd.print("State: IDLE     "); // Spaces to clear any residual text

  // Initialize debounce variables
  debouncedLevelState = false;
  lastLevelChangeTime = millis();
}

void loop() {
  // Check wash standby switch and handle state transitions
  if (digitalRead(washStandbyPin) == SWITCH_ON && currentState != WASH) {
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
}

// Functions to handle each state

void idleState() {
  turnAllRelays(RELAY_OFF);
  if (!isLevelReached()) {
    updateState(MIXING);
  }
}

void mixingState() {
    // Initialize mixing start time if just entering mixing state
    static bool initializeTimer = true;
    if (initializeTimer) {
        mixingStart = millis();
        initializeTimer = false;
        timeoutOccurred = false;
    }
    
    // Check for timeout
    if ((millis() - mixingStart) >= mixingTimeout) {
        timeoutOccurred = true;
        displayErrorMessage("Mixing Timeout");
        updateState(ERROR);
        initializeTimer = true;  // Reset for next time
        return;
    }
    
    turnAllRelays(RELAY_ON);
    if (isLevelReached()) {
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
    digitalWrite(mixerPin, RELAY_ON);
    digitalWrite(waterPin, RELAY_OFF);
    digitalWrite(augerPin, RELAY_OFF);
    digitalWrite(agitatorPin, RELAY_OFF);

    if (millis() - postMixingStart >= postMixingDuration) {
        updateState(IDLE);
    }
}

void washState() {
  turnAllRelays(RELAY_OFF);

  if (digitalRead(washDispensePin) == SWITCH_ON) {
    digitalWrite(waterPin, RELAY_ON);  // Dispense water
  } else {
    digitalWrite(waterPin, RELAY_OFF); // Turn off water dispenser
  }

  if (digitalRead(washStandbyPin) == SWITCH_OFF) {
    updateState(IDLE);
  }
}

void errorState() {
    turnAllRelays(RELAY_OFF);
    
    // Wait for wash switch toggle
    if (digitalRead(washStandbyPin) == SWITCH_ON) {
        timeoutOccurred = false;  // Clear the timeout flag
        errorMessageDisplayed = false;  // Reset the display flag
        updateState(IDLE);
    }
}

// Relay Control Functions
void turnAllRelays(uint8_t state) {
  digitalWrite(mixerPin, state);
  digitalWrite(waterPin, state);
  digitalWrite(augerPin, state);
  digitalWrite(agitatorPin, state);
}

// Function to update the current state and print state change if necessary
void updateState(State newState) {
    if (newState != currentState) {
        currentState = newState;
        errorMessageDisplayed = false;  // Reset the flag on state change

        // Rest of your existing updateState function remains the same
        Serial.print("State changed to: ");
        switch (currentState) {
      case IDLE: Serial.println("IDLE"); break;
      case MIXING: Serial.println("MIXING"); break;
      case POST_MIXING: Serial.println("POST_MIXING"); break;
      case WASH: Serial.println("WASH"); break;
      case ERROR: Serial.println("ERROR"); break;
    }

    // Update LCD Display
    lcd.clear();
    lcd.setCursor(0, 0);

    switch (currentState) {
      case IDLE:
        setLCDColor(0, 255, 0);  // Green
        lcd.print("State: IDLE     ");
        break;
      case MIXING:
        setLCDColor(0, 0, 255);  // Blue
        lcd.print("State: MIXING   ");
        break;
      case POST_MIXING:
        setLCDColor(0, 255, 255);  // Cyan
        lcd.print("State: POST-MIX ");
        break;
      case WASH:
        setLCDColor(255, 165, 0);  // Orange
        lcd.print("State: WASH     ");
        break;
      case ERROR:
        setLCDColor(255, 0, 0);  // Red
        lcd.print("State: ERROR    ");
        break;
      default:
        setLCDColor(255, 0, 0);  // Red
        lcd.print("UNKNOWN STATE   ");
        break;
    }
  }
}

// Liquid Level Function with Aggressive Debouncing
bool isLevelReached() {
  static bool lastRawLevelState = SWITCH_OFF;
  bool currentRawLevelState = digitalRead(liquidLevelPin);

  if (currentRawLevelState != lastRawLevelState) {
    lastLevelChangeTime = millis(); // Reset the debounce timer
  }

  if ((millis() - lastLevelChangeTime) >= debounceDelay) {
    if (currentRawLevelState != debouncedLevelState) {
      debouncedLevelState = currentRawLevelState;
      Serial.print("Liquid level state changed to: ");
      Serial.println(debouncedLevelState == SWITCH_ON ? "REACHED" : "NOT REACHED");
    }
  }

  lastRawLevelState = currentRawLevelState;
  return debouncedLevelState == SWITCH_ON;
}

// LCD Helper Functions
void setLCDColor(int r, int g, int b) {
  lcd.setRGB(r, g, b);
}

void displayMessage(const char* message) {
  lcd.clear();
  lcd.print(message);
}

void displayErrorMessage(const char* errorMessage) {
    if (!errorMessageDisplayed) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(errorMessage);
        lcd.setCursor(0, 1);
        lcd.print("Toggle WashSwtch");
        errorMessageDisplayed = true;
    }
}
