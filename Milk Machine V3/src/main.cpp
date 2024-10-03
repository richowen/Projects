#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "home_assistant.h"

// Global variables
State currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long mixingStartTime = 0;
bool isError = false;
bool sensorActivatedDuringPostMix = false;

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

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for Serial to be ready
    }
    Serial.println("Serial communication initialized");
    
    setupPins();
    setupLCD();
    setupWiFi();  // Make sure this sets up the WiFi connection
    setupHomeAssistant();
    digitalWrite(ledPin, HIGH);
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

  // Check if error condition is resolved
  if (!readDebouncedSensor()) {
    isError = false;
    transitionTo(IDLE);
  }
}

void transitionTo(State newState) {
    currentState = newState;
    stateStartTime = millis();

    if (newState == MIXING) {
        mixingStartTime = millis();
    }

    const char* stateStr = getStateString(newState);
    updateLCD(currentState);
    updateDevices(newState);
    
    // Update Home Assistant
    updateHomeAssistant(stateStr);
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
  if (readDebouncedSensor()) {
    transitionTo(WAITING_PRE_MIX);
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

void loop() {
    loopHomeAssistant();

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
    updateLCD(currentState);
    delay(50);
}