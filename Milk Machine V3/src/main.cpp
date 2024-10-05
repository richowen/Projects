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
bool isError = false;
bool sensorActivatedDuringPostMix = false;
int hopperLevel = 0;

// Function declarations
void setupPins();
void handleIdleState();
void handleWaitingPreMixState();
void handleMixingState();
void handleWaitingPostMixState();
void handleErrorState();
void transitionTo(State newState);
void updateLED();
const char* getStateString(State state);
void updateDevices(State state);
bool readDebouncedSensor();
void updateLCDWithSensorInfo();

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for Serial to be ready
    }
    Serial.println("Serial communication initialized");
    
    Wire.begin();  // Initialize I2C
    delay(100);  // Give some time for I2C bus to stabilize
    
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

  // Initialize all devices to off
  digitalWrite(mixerPin, HIGH);
  digitalWrite(waterPin, HIGH);
  digitalWrite(augerPin, HIGH);
  digitalWrite(agitatorPin, HIGH);
}

void handleWaitingPreMixState() {
  if (!readDebouncedSensor()) {
    transitionTo(IDLE);
  } else if (millis() - stateStartTime >= waitingDuration) {
    transitionTo(MIXING);
  }
}

void handleMixingState() {
  if (!readDebouncedSensor()) {
    sensorActivatedDuringPostMix = false;
    transitionTo(WAITING_POST_MIX);
  } else if (millis() - mixingStartTime >= maxMixingDuration) {
    transitionTo(ERROR);
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

void handleErrorState() {
  // In error state, all devices should be off
  digitalWrite(mixerPin, HIGH);
  digitalWrite(waterPin, HIGH);
  digitalWrite(augerPin, HIGH);
  digitalWrite(agitatorPin, HIGH);

  // Check if the error is due to low hopper level
  if (isHopperLow()) {
    updateLCD(ERROR, hopperLevel, "Low Hopper");
    
    // Check if hopper has been refilled
    if (!isHopperLow()) {
      isError = false;
      transitionTo(IDLE);
    }
  } else {
    // Handle other types of errors (e.g., mixing time exceeded)
    updateLCD(ERROR, hopperLevel, "Mix time Exceeded");

    // Check if error condition is resolved (original condition)
    if (!readDebouncedSensor()) {
      isError = false;
      transitionTo(IDLE);
    }
  }
}

void transitionTo(State newState) {
    currentState = newState;
    stateStartTime = millis();

    if (newState == MIXING) {
        mixingStartTime = millis();
    }

    const char* stateStr = getStateString(newState);
    updateLCD(currentState, hopperLevel);
    updateDevices(newState);
}

void updateLED() {
  if (isError) {
    // Flash LED during error state
    digitalWrite(ledPin, (millis() / 500) % 2);
  } else {
    // Keep LED on as power indicator in all other states
    digitalWrite(ledPin, HIGH);
  }
}

const char* getStateString(State state) {
  switch (state) {
    case IDLE: return "idle";
    case WAITING_PRE_MIX: return "waiting_pre_mix";
    case MIXING: return "mixing";
    case WAITING_POST_MIX: return "waiting_post_mix";
    case ERROR: return "error";
    default: return "unknown";
  }
}

void updateDevices(State state) {
  switch (state) {
    case IDLE:
    case WAITING_PRE_MIX:
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
    case ERROR:
      digitalWrite(mixerPin, HIGH);
      digitalWrite(waterPin, HIGH);
      digitalWrite(augerPin, HIGH);
      digitalWrite(agitatorPin, HIGH);
      break;
  }
}

void handleIdleState() {
  if (readDebouncedSensor() && !isHopperLow()) {
    transitionTo(WAITING_PRE_MIX);
  } else if (isHopperLow()) {
    displayMessage("Hopper Low!");
    delay(2000);
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

unsigned long lastLCDUpdate = 0;
State lastReportedState = IDLE;
int lastReportedHopperLevel = -1;  // Initialize to an impossible value to ensure first update

void loop() {
    loopHomeAssistant();
    hopperLevel = readHopperLevel();

    // Check for low hopper level
    if (isHopperLow() && currentState != ERROR) {
        transitionTo(ERROR);
    }

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
        case ERROR:
            handleErrorState();
            break;
    }
    
    updateLED();
    
    // Update LCD if the interval has passed
    unsigned long currentMillis = millis();
    if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        if (currentState == ERROR) {
            if (isHopperLow()) {
                updateLCD(currentState, hopperLevel, "Low Hopper");
            } else {
                updateLCD(currentState, hopperLevel, "Mix time");
            }
        } else {
            updateLCD(currentState, hopperLevel);
        }
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