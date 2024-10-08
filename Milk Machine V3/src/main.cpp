#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"
#include "laser_sensor.h"
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

// Function declarations
void setupPins();
void handleIdleState();
void handleWaitingPreMixState();
void handleMixingState();
void handleWaitingPostMixState();
void handleErrorState();
void handleWashStandbyState();
void handleWashDispenseState();
void transitionTo(State newState);
void updateLED();
const char * getStateString(State state);
void updateDevices(State state);
bool readDebouncedSensor();
void updateLCDWithSensorInfo();
State readWashMode();
bool checkForErrors();

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for Serial to be ready
  }
  Serial.println("Serial communication initialized");

  Wire.begin(); // Initialize I2C
  delay(100); // Give some time for I2C bus to stabilize

  setupPins();
  Serial.println("Pins setup complete");

  setupLCD();
  Serial.println("LCD setup complete");

  setupLaserSensor();
  Serial.println("Laser sensor setup complete");

  setupWiFi();
  Serial.println("WiFi setup complete");

  setupHomeAssistant();
  Serial.println("Home Assistant setup complete");

  digitalWrite(ledPin, HIGH);
  Serial.println("Setup complete");
}

void setupPins() {
  pinMode(mixerPin, OUTPUT);
  pinMode(waterPin, OUTPUT);
  pinMode(augerPin, OUTPUT);
  pinMode(agitatorPin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(washStandbyPin, INPUT_PULLUP);
  pinMode(washDispensePin, INPUT_PULLUP);

  // Initialize all devices to off
  digitalWrite(mixerPin, HIGH);
  digitalWrite(waterPin, HIGH);
  digitalWrite(augerPin, HIGH);
  digitalWrite(agitatorPin, HIGH);
}

void updateDevices(State state) {
  switch (state) {
  case ERROR:
  case IDLE:
  case WAITING_PRE_MIX:
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

void handleIdleState() {
  if (readDebouncedSensor() && !isHopperLow()) {
    transitionTo(WAITING_PRE_MIX);
  } else if (isHopperLow()) {
    currentErrors |= ERROR_HOPPER_LOW; // Set error flag for low hopper
    transitionTo(ERROR); // Move to error state
  }
}

void handleWaitingPreMixState() {
  if (!readDebouncedSensor()) {
    transitionTo(IDLE);
  } else if (millis() - stateStartTime >= waitingDuration) {
    transitionTo(MIXING);
  }
}

void handleMixingState() {
  // Check if the sensor is no longer activated
  if (!readDebouncedSensor()) {
    sensorActivatedDuringPostMix = false;
    transitionTo(WAITING_POST_MIX);
  } 
  // Check if mixing time exceeded the maximum allowed duration
  else if (millis() - mixingStartTime >= maxMixingDuration) {
    currentErrors |= ERROR_MIX_TIME_EXCEEDED; // Set error flag
    transitionTo(ERROR); // Move to error state
  } 
  // Check if the hopper level is low during mixing
  else if (isHopperLow()) {
    currentErrors |= ERROR_HOPPER_LOW; // Set error flag for low hopper
    transitionTo(ERROR); // Move to error state
  }
}

void handleWaitingPostMixState() {
  if (readDebouncedSensor()) {
    sensorActivatedDuringPostMix = true;
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
  updateDevices(WASH_STANDBY);
}

void handleWashDispenseState() {
  updateDevices(WASH_DISPENSE);
}

void handleErrorState() {
  updateDevices(ERROR);

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
  uint8_t newErrors = 0;

  if (isHopperLow()) {
    newErrors |= ERROR_HOPPER_LOW;
  }

  if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
    newErrors |= ERROR_MIX_TIME_EXCEEDED; // Keep this error active
  }

  currentErrors = newErrors;

  if (currentErrors == 0) {
    transitionTo(IDLE);
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

void transitionTo(State newState) {
  currentState = newState;
  stateStartTime = millis();

  if (newState == MIXING) {
    mixingStartTime = millis();
  }

  const char * stateStr = getStateString(newState);
  Serial.printf("Transitioning to state: %s\n", stateStr);
  updateLCD(currentState, hopperLevel);
  updateDevices(newState);
}

void updateLED() {
  if (currentState == ERROR) {
    // Flash LED during error state
    digitalWrite(ledPin, (millis() / 500) % 2);
  } else {
    // Keep LED on as power indicator in all other states
    digitalWrite(ledPin, HIGH);
  }
}

const char * getStateString(State state) {
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

bool readDebouncedSensor() {
  static unsigned long lastDebounceTime = 0;
  static int lastSteadyState = LOW;
  static int lastFlickerableState = LOW;

  int currentState = digitalRead(sensorPin);
  unsigned long currentTime = millis();

  if (currentState != lastFlickerableState) {
    lastDebounceTime = currentTime;
    lastFlickerableState = currentState;
  }

  if ((currentTime - lastDebounceTime) > debounceDelay) {
    if (lastSteadyState != currentState) {
      lastSteadyState = currentState;
    }
  }

  return lastSteadyState == HIGH;
}

void updateLCDWithSensorInfo() {
  clearLCD();
  setCursor(0, 0);
  printLCD(getStateString(currentState));

  setCursor(0, 1);
  printLCD("Hopper: ");
  printLCD(hopperLevel);
  printLCD("%");
}

State readWashMode() {
  bool standbyActive = digitalRead(washStandbyPin) == LOW;
  bool dispenseActive = digitalRead(washDispensePin) == LOW;

  if (standbyActive && dispenseActive) {
    return WASH_DISPENSE;
  } else if (standbyActive && !dispenseActive) {
    return WASH_STANDBY;
  } else {
    return IDLE; // Normal operation when both switches are open or only dispense is active
  }
}

void loop() {
  loopHomeAssistant();
  hopperLevel = readHopperLevel();

  // Always check for errors
  if (checkForErrors()) {
    if (currentState != ERROR) {
      transitionTo(ERROR); // Ensure we only transition to error once
    }
    handleErrorState();
    return; // Prevent further state transitions in case of an error
  }

  State washMode = readWashMode();
  switch (washMode) {
  case IDLE:
    if (currentState == WASH_STANDBY || currentState == WASH_DISPENSE) {
      transitionTo(IDLE);
    } else {
      // Normal operation
      switch (currentState) {
      case IDLE:
        handleIdleState();
        break;
      case WAITING_PRE_MIX:
        handleWaitingPreMixState();
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

  updateLED();

  // Update LCD if the interval has passed
  unsigned long currentMillis = millis();
  if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    updateLCD(currentState, hopperLevel);
    lastLCDUpdate = currentMillis;
  }

  // Update Home Assistant if state or hopper level has changed
  if (currentState != lastReportedState || hopperLevel != lastReportedHopperLevel) {
    updateHomeAssistant(getStateString(currentState), hopperLevel);
    lastReportedState = currentState;
    lastReportedHopperLevel = hopperLevel;
  }

  delay(50);
}