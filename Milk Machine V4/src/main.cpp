#include <Arduino.h>
#include <Wire.h>

// Relay control pins
const int mixerPin = 16;
const int waterPin = 17;
const int augerPin = 25;
const int agitatorPin = 26;

// Wash mode switches
const int washStandbyPin = 23;    // Wash mode toggle switch
const int washDispensePin = 5;     // Wash water dispense switch

// Sensor addresses (assuming I2C addresses)
//const uint8_t liquidLevelSensorAddress = 0xXX; // Replace with actual address
//const uint8_t powderLevelSensorAddress = 0xXX; // Replace with actual address

// Desired liquid level threshold
const float desiredLiquidLevel = 100.0; // Adjust based on your sensor's units

// Mixer run-on time after dispensing stops (in milliseconds)
const unsigned long mixerRunOnTime = 5000;

// State definitions
enum MachineState {
  IDLE,
  MIXING,
  WASH_MODE
};

MachineState currentState = IDLE;

// Timing variables
unsigned long mixerStopTime = 0;

// Flags
bool mixerRunOn = false;

// Function prototypes
void handleIdleState();
void handleMixingState();
void handleWashMode(bool isWashDispense);
void startDispensing();
void stopDispensing();
void stopMixer();
float readLiquidLevelSensor();
void initializeSensors();

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Initialize relay control pins
  pinMode(mixerPin, OUTPUT);
  pinMode(waterPin, OUTPUT);
  pinMode(augerPin, OUTPUT);
  pinMode(agitatorPin, OUTPUT);

  // Initialize wash mode switches
  pinMode(washStandbyPin, INPUT_PULLUP);
  pinMode(washDispensePin, INPUT_PULLUP);

  // Initialize all relays to OFF
  digitalWrite(mixerPin, LOW);
  digitalWrite(waterPin, LOW);
  digitalWrite(augerPin, LOW);
  digitalWrite(agitatorPin, LOW);

  // Initialize I2C communication for sensors
  Wire.begin();

  // Initialize sensors
  initializeSensors();
}

void loop() {
  // Read wash mode switches
  bool isWashMode = digitalRead(washStandbyPin) == LOW;      // Assuming LOW when switch is ON
  bool isWashDispense = digitalRead(washDispensePin) == LOW; // Assuming LOW when switch is ON

  // State Machine Logic
  if (isWashMode) {
    currentState = WASH_MODE;
  } else {
    // If we were in WASH_MODE and wash mode is turned off
    if (currentState == WASH_MODE) {
      currentState = IDLE; // Return to IDLE when wash mode is turned off
    }
  }

  switch (currentState) {
    case IDLE:
      handleIdleState();
      break;

    case MIXING:
      handleMixingState();
      break;

    case WASH_MODE:
      handleWashMode(isWashDispense);
      break;
  }

  // Handle mixer run-on timing
  if (mixerRunOn && millis() >= mixerStopTime) {
    stopMixer();
    mixerRunOn = false;
  }

  // Add a small delay to avoid overwhelming the loop
  delay(100);
}

// State handling functions

void handleIdleState() {
  // Read liquid level sensor
  float liquidLevel = readLiquidLevelSensor();
  Serial.print("Liquid Level: ");
  Serial.println(liquidLevel);

  if (liquidLevel < desiredLiquidLevel) {
    currentState = MIXING;
  }

  // Ensure all outputs are off
  stopDispensing();
  stopMixer();
}

void handleMixingState() {
  // Start dispensing components
  startDispensing();

  // Read liquid level sensor
  float liquidLevel = readLiquidLevelSensor();
  Serial.print("Liquid Level: ");
  Serial.println(liquidLevel);

  if (liquidLevel >= desiredLiquidLevel) {
    // Stop dispensing powder and water, keep mixer running
    stopDispensing();

    // Keep mixer running for additional time
    mixerRunOn = true;
    mixerStopTime = millis() + mixerRunOnTime;

    // Return to IDLE state; mixer will stop after run-on time
    currentState = IDLE;
  }
}

void handleWashMode(bool isWashDispense) {
  // Stop all normal operations
  stopDispensing();
  stopMixer();

  if (isWashDispense) {
    // Dispense water
    digitalWrite(waterPin, HIGH);
  } else {
    // Stop water dispensing
    digitalWrite(waterPin, LOW);
  }
}

// Helper functions

void startDispensing() {
  digitalWrite(mixerPin, HIGH);      // Start mixer
  digitalWrite(waterPin, HIGH);      // Open water solenoid
  digitalWrite(augerPin, HIGH);      // Start auger
  digitalWrite(agitatorPin, HIGH);   // Start agitator
}

void stopDispensing() {
  digitalWrite(waterPin, LOW);       // Close water solenoid
  digitalWrite(augerPin, LOW);       // Stop auger
  digitalWrite(agitatorPin, LOW);    // Stop agitator
  // Mixer remains ON if mixerRunOn is true
}

void stopMixer() {
  digitalWrite(mixerPin, LOW);       // Stop mixer
}

float readLiquidLevelSensor() {
  // Implement sensor reading logic
  // Placeholder for actual sensor code
  // For now, return a simulated value
  // Replace this with actual sensor code
  float simulatedLevel = analogRead(A0); // Assuming analog input for simulation
  return simulatedLevel;
}

void initializeSensors() {
  // Initialize liquid level sensor
  // Replace with actual initialization code for your sensor
  // Example:
  // liquidLevelSensor.begin();
}

// Additional functions for powder level sensor, LCD display, etc., can be added here
