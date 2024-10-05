#include <Arduino.h>          // Arduino core library
#include "config.h"           // Custom configuration settings
#include "wifi_manager.h"     // WiFi management functions
#include "lcd_manager.h"      // LCD control functions
#include "home_assistant.h"   // Home Assistant integration
#include "laser_sensor.h"     // Laser sensor control
#include <Wire.h>             // I2C communication library

// Global variables
State currentState = IDLE;    // System's current state, starts as IDLE
unsigned long stateStartTime = 0;  // Timestamp when state started
unsigned long mixingStartTime = 0; // Timestamp when mixing started
bool sensorActivatedDuringPostMix = false; // Tracks if sensor was triggered after mixing
int hopperLevel = 0;          // Level of hopper (ingredient container)
unsigned long lastLCDUpdate = 0;  // Time of last LCD update
State lastReportedState = IDLE;   // Last reported state to Home Assistant
int lastReportedHopperLevel = -1; // Last reported hopper level
uint8_t currentErrors = 0;        // Current error flags
const uint8_t ERROR_HOPPER_LOW = 0x01;       // Error code for low hopper level
const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02; // Error code for exceeded mix time

// Function declarations
void setupPins();                    // Configures the hardware pins
void handleIdleState();              // Handles behavior in IDLE state
void handleWaitingPreMixState();     // Handles behavior before mixing
void handleMixingState();            // Handles mixing state
void handleWaitingPostMixState();    // Handles behavior after mixing
void handleErrorState();             // Handles error conditions
void handleWashStandbyState();       // Handles wash standby state
void handleWashDispenseState();      // Handles wash dispense state
void transitionTo(State newState);   // Transition to a new state
void updateLED();                    // Updates LED status based on system state
const char * getStateString(State state); // Returns state name as a string
void updateDevices(State state);     // Updates devices (mixer, water, etc.) based on state
bool readDebouncedSensor();          // Reads sensor input with debouncing
void updateLCDWithSensorInfo();      // Updates the LCD with sensor and system info
State readWashMode();                // Determines the current wash mode
bool checkForErrors();               // Checks for system errors

void setup() {
  Serial.begin(115200);              // Initialize serial communication at 115200 baud
  while (!Serial) {
    ; // Wait for Serial to be ready
  }
  Serial.println("Serial communication initialized");

  Wire.begin();                      // Initialize I2C bus for communication
  delay(100);                        // Delay to stabilize the I2C bus

  setupPins();                       // Set up the pins for various devices
  Serial.println("Pins setup complete");

  setupLCD();                        // Set up the LCD screen
  Serial.println("LCD setup complete");

  setupLaserSensor();                // Set up the laser sensor for measuring hopper level
  Serial.println("Laser sensor setup complete");

  setupWiFi();                       // Connect to WiFi network
  Serial.println("WiFi setup complete");

  setupHomeAssistant();              // Initialize communication with Home Assistant
  Serial.println("Home Assistant setup complete");

  digitalWrite(ledPin, HIGH);        // Turn on the LED to indicate system power
  Serial.println("Setup complete");
}

void setupPins() {
  // Configure hardware pins as input/output
  pinMode(mixerPin, OUTPUT);
  pinMode(waterPin, OUTPUT);
  pinMode(augerPin, OUTPUT);
  pinMode(agitatorPin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);  // Pull-up resistor for sensor input
  pinMode(ledPin, OUTPUT);
  pinMode(washStandbyPin, INPUT_PULLUP);   // Pull-up for wash standby button
  pinMode(washDispensePin, INPUT_PULLUP);  // Pull-up for wash dispense button

  // Initialize devices to off (HIGH means off for some devices)
  digitalWrite(mixerPin, HIGH);
  digitalWrite(waterPin, HIGH);
  digitalWrite(augerPin, HIGH);
  digitalWrite(agitatorPin, HIGH);
}

void handleIdleState() {
  if (readDebouncedSensor() && !isHopperLow()) {
    // If the sensor is activated and the hopper is not low, transition to pre-mix state
    transitionTo(WAITING_PRE_MIX);
  } else if (isHopperLow()) {
    // If the hopper is low, display a warning message
    displayMessage("Hopper Low!");
    delay(2000);  // Wait for 2 seconds
  }
}

void handleWaitingPreMixState() {
  if (!readDebouncedSensor()) {
    // If the sensor is deactivated, return to idle state
    transitionTo(IDLE);
  } else if (millis() - stateStartTime >= waitingDuration) {
    // If enough time has passed, transition to mixing state
    transitionTo(MIXING);
  }
}

void handleMixingState() {
  if (!readDebouncedSensor()) {
    // If sensor is deactivated, move to post-mix state
    sensorActivatedDuringPostMix = false;
    transitionTo(WAITING_POST_MIX);
  } else if (millis() - mixingStartTime >= maxMixingDuration) {
    // If mixing time exceeds limit, trigger error
    currentErrors |= ERROR_MIX_TIME_EXCEEDED;
    transitionTo(ERROR);  // Transition to error state
  }
}

void handleWaitingPostMixState() {
  if (readDebouncedSensor()) {
    sensorActivatedDuringPostMix = true;  // Record sensor activation
  }

  if (millis() - stateStartTime >= waitingDuration) {
    // If enough time has passed, decide next state
    if (sensorActivatedDuringPostMix) {
      transitionTo(MIXING);  // Restart mixing if sensor was activated
    } else {
      transitionTo(IDLE);    // Go back to idle if no sensor activity
    }
  }
}

bool checkForErrors() {
  uint8_t newErrors = 0;

  if (isHopperLow()) {
    // Set hopper low error flag
    newErrors |= ERROR_HOPPER_LOW;
  }

  if (currentState == MIXING && (millis() - mixingStartTime >= maxMixingDuration)) {
    // Set mixing time exceeded error flag
    newErrors |= ERROR_MIX_TIME_EXCEEDED;
  }

  if (currentState != ERROR) {
    currentErrors = newErrors;  // Update error state only if not already in error
  }

  return currentErrors != 0;  // Return true if errors exist
}

void handleErrorState() {
  updateDevices(ERROR);   // Turn off devices during error state

  // Display error messages on LCD
  clearLCD();
  setCursor(0, 0);
  printLCD("ERROR:");
  setCursor(0, 1);

  // Show error based on flags
  if (currentErrors & ERROR_HOPPER_LOW) {
    printLCD("Low Hopper");
  }
  if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
    printLCD("Mix Time Exceeded");
  }

  // Check for persistent errors
  uint8_t newErrors = 0;
  if (isHopperLow()) {
    newErrors |= ERROR_HOPPER_LOW;
  }
  if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
    newErrors |= ERROR_MIX_TIME_EXCEEDED;
  }

  currentErrors = newErrors;  // Update current errors

  if (currentErrors == 0) {
    // Transition back to idle if all errors are cleared
    transitionTo(IDLE);
  }
}

void handleWashStandbyState() {
  updateDevices(WASH_STANDBY);   // Turn off devices in wash standby
}

void handleWashDispenseState() {
  updateDevices(WASH_DISPENSE);  // Only turn on water in wash dispense mode
}

void transitionTo(State newState) {
  currentState = newState;      // Update the current state
  stateStartTime = millis();    // Record the time the state started

  if (newState == MIXING) {
    mixingStartTime = millis();  // Record the start time for mixing
  }

  // Print state transition message to serial monitor
  const char * stateStr = getStateString(newState);
  Serial.printf("Transitioning to state: %s\n", stateStr);

  updateLCD(currentState, hopperLevel);  // Update the LCD with new state info
  updateDevices(newState);               // Update device outputs based on state
}

void updateLED() {
  if (currentState == ERROR) {
    // Flash LED if the system is in error state
    digitalWrite(ledPin, (millis() / 500) % 2);
  } else {
    // Keep LED on in other states
    digitalWrite(ledPin, HIGH);
  }
}

const char * getStateString(State state) {
  // Returns a string representation of the current state
  switch (state) {
  case IDLE:
    return "idle";
  case WAITING_PRE_MIX:
    return "waiting_pre_mix";
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

void updateDevices(State state) {
  // Update device outputs (e.g., mixer, water, auger) based on state
  switch (state) {
  case ERROR:
  case IDLE:
  case WAITING_PRE_MIX:
  case WASH_STANDBY:
    // Turn all devices off
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
    break;
  case MIXING:
    // Turn on all devices for mixing
    digitalWrite(mixerPin, LOW);
    digitalWrite(waterPin, LOW);
    digitalWrite(augerPin, LOW);
    digitalWrite(agitatorPin, LOW);
    break;
  case WAITING_POST_MIX:
    // Keep mixer on, turn off other devices
    digitalWrite(mixerPin, LOW);
    digitalWrite(waterPin, HIGH);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
    break;
  case WASH_DISPENSE:
    // Only turn on water for washing
    digitalWrite(mixerPin, HIGH);
    digitalWrite(waterPin, LOW);
    digitalWrite(augerPin, HIGH);
    digitalWrite(agitatorPin, HIGH);
    break;
  }
}

bool readDebouncedSensor() {
  // Debounce logic to prevent noisy sensor readings
  static unsigned long lastDebounceTime = 0;
  static int lastSteadyState = LOW;  // Last stable state
  static int lastFlickerableState = LOW;  // Temporary flickering state

  int currentState = digitalRead(sensorPin);  // Read current sensor state
  unsigned long currentTime = millis();       // Get current time

  if (currentState != lastFlickerableState) {
    // Reset debounce timer if sensor state changed
    lastDebounceTime = currentTime;
    lastFlickerableState = currentState;
  }

  if ((currentTime - lastDebounceTime) > debounceDelay) {
    // If enough time has passed, update steady state
    if (lastSteadyState != currentState) {
      lastSteadyState = currentState;
    }
  }

  return lastSteadyState == HIGH;  // Return true if sensor is stable and HIGH
}

void updateLCDWithSensorInfo() {
  // Clear and update LCD with current state and hopper level
  clearLCD();
  setCursor(0, 0);
  printLCD(getStateString(currentState));

  setCursor(0, 1);
  printLCD("Hopper: ");
  printLCD(hopperLevel);
  printLCD("%");
}

State readWashMode() {
  // Read the wash mode based on switch inputs
  bool standbyActive = digitalRead(washStandbyPin) == LOW;
  bool dispenseActive = digitalRead(washDispensePin) == LOW;

  if (standbyActive && dispenseActive) {
    return WASH_DISPENSE;  // Both switches activated
  } else if (standbyActive && !dispenseActive) {
    return WASH_STANDBY;   // Only standby switch activated
  } else {
    return IDLE;           // Default to normal operation
  }
}

void loop() {
  loopHomeAssistant();  // Keep Home Assistant connected and updated
  hopperLevel = readHopperLevel();  // Update hopper level from sensor

  // Check for errors before processing states
  if (checkForErrors()) {
    if (currentState != ERROR) {
      transitionTo(ERROR);  // Enter error state if an error is detected
    }
    handleErrorState();     // Handle error conditions
    return;  // Exit early if there's an error
  }

  State washMode = readWashMode();  // Determine if we're in a wash mode
  switch (washMode) {
  case IDLE:
    // Handle normal operation
    switch (currentState) {
    case IDLE:
      handleIdleState();  // Manage idle behavior
      break;
    case WAITING_PRE_MIX:
      handleWaitingPreMixState();  // Manage pre-mixing behavior
      break;
    case MIXING:
      handleMixingState();  // Manage mixing behavior
      break;
    case WAITING_POST_MIX:
      handleWaitingPostMixState();  // Manage post-mixing behavior
      break;
    }
    break;
   case WASH_STANDBY:
    if (currentState != WASH_STANDBY) {
      // Transition to wash standby state if not already in it
      transitionTo(WASH_STANDBY);
    }
    handleWashStandbyState(); // Manage operations in wash standby state
    break;
  
  case WASH_DISPENSE:
    if (currentState != WASH_DISPENSE) {
      // Transition to wash dispense state if not already in it
      transitionTo(WASH_DISPENSE);
    }
    handleWashDispenseState(); // Manage operations in wash dispense state
    break;
  }

  updateLED(); // Update LED status based on the current state

  // Update LCD display if the designated interval has passed
  unsigned long currentMillis = millis(); // Get current time in milliseconds
  if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    updateLCD(currentState, hopperLevel); // Update LCD with current state and hopper level
    lastLCDUpdate = currentMillis; // Record the last update time
  }

  // Update Home Assistant if the state or hopper level has changed
  if (currentState != lastReportedState || hopperLevel != lastReportedHopperLevel) {
    // Notify Home Assistant with the current state and hopper level
    updateHomeAssistant(getStateString(currentState), hopperLevel);
    lastReportedState = currentState; // Store the last reported state
    lastReportedHopperLevel = hopperLevel; // Store the last reported hopper level
  }

  delay(50); // Delay to limit loop execution speed (reduces CPU load)
}
