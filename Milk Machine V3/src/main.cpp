#include <Arduino.h>

// Define pin connections
const int mixerpin = 26;   // Auger DC motor
const int waterpin = 25;  // Agitator DC motor
const int augerpin = 16;   // 240V AC solenoid
const int agitatorpin = 17;   // 240V AC mixer motor
const int sensorPin = 18;   // Liquid level sensor
const int ledpin = 19;

void setup() {
  // Set pin modes
  pinMode(mixerpin, OUTPUT);
  pinMode(waterpin, OUTPUT);
  pinMode(augerpin, OUTPUT);
  pinMode(agitatorpin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);
  pinMode(ledpin, OUTPUT);

  // Initialize all devices to off
  digitalWrite(mixerpin, HIGH);
  digitalWrite(waterpin, HIGH);
  digitalWrite(augerpin, HIGH);
  digitalWrite(agitatorpin, HIGH);

  digitalWrite(ledpin, HIGH);
}

void loop() {
  static unsigned long offTime = 0; // Time when pins were turned off
  static unsigned long onTime = 0;  // Time when sensor reads HIGH
  const unsigned long mixerOnDuration = 5000; // Mixer on duration in milliseconds
  const unsigned long feedDelay = 5000;       // Delay before turning on devices

  // Check if the liquid level sensor reads HIGH
  if (digitalRead(sensorPin) == HIGH) {
    // Check if this is the first time sensor reads HIGH
    if (onTime == 0) {
      onTime = millis(); // Store the time when sensor reads HIGH
    } else if (millis() - onTime > feedDelay) {
      // If the delay has passed, turn on all devices
      digitalWrite(mixerpin, LOW);
      digitalWrite(waterpin, LOW);
      digitalWrite(augerpin, LOW);
      digitalWrite(agitatorpin, LOW);
      offTime = 0; // Reset the off time
    }
  } else { // If the sensor reads LOW
    onTime = 0; // Reset the on time
    digitalWrite(waterpin, HIGH);
    digitalWrite(augerpin, HIGH);
    digitalWrite(agitatorpin, HIGH);

    // Check if this is the first time turning off
    if (offTime == 0) {
      offTime = millis(); // Store the time when devices were turned off
    } else if (millis() - offTime > mixerOnDuration) {
      // If the mixer has been on long enough, turn it off
      digitalWrite(mixerpin, HIGH);
    }
  }
  
  // Safety feature: Add a delay to prevent rapid switching
  delay(1000); // 1-second delay
}


