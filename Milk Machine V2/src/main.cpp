#include <Arduino.h>

// Constants and Pin Configuration
const int liquidSensorPin = 13;
const int mixerMotorRelayPin = 12;
const int waterSolenoidRelayPin = 11;
const int motor1DirectionPin = 5; // Auger motor
const int motor2DirectionPin = 6; // Agitator motor
const unsigned long sensorHighTimeout = 10000; // Timeout duration in milliseconds, set to 10 seconds

// Helper Functions Declarations
void dispenseWater(bool state);
void operateMixer(bool state);
void operateAuger(bool state);
void operateAgitator(bool state);
void mixForDuration(unsigned long duration);

void setup() {
    // Initialize serial communication at 9600 bits per second
    Serial.begin(9600);
    // Enable internal pull-up resistor
    pinMode(liquidSensorPin, INPUT_PULLUP);
    
    pinMode(mixerMotorRelayPin, OUTPUT);
    pinMode(waterSolenoidRelayPin, OUTPUT);
    pinMode(motor1DirectionPin, OUTPUT);
    pinMode(motor2DirectionPin, OUTPUT);
    
    // Initialize relays to OFF position
    digitalWrite(mixerMotorRelayPin, LOW);
    digitalWrite(waterSolenoidRelayPin, LOW);
    digitalWrite(motor1DirectionPin, LOW);
    digitalWrite(motor2DirectionPin, LOW);
}

void loop() {

    // Check for low milk level
    if (digitalRead(liquidSensorPin) == HIGH) {
        // Start dispensing process
        dispenseWater(true);
        operateAuger(true);
        operateAgitator(true);
        operateMixer(true);

        // Wait until the milk level reaches the sensor
        while(digitalRead(liquidSensorPin) == HIGH);

        // Stop dispensing water and powder, continue mixing
        dispenseWater(false);
        operateAuger(false);
        operateAgitator(false);

        // Mix for additional 5 seconds
        mixForDuration(5000);
    }

    // Go back to idle state
    delay(3000); // Delay for stability, adjust as needed
}

// Helper Functions Definitions
void dispenseWater(bool state) {
    digitalWrite(waterSolenoidRelayPin, state ? HIGH : LOW);
}

void operateMixer(bool state) {
    digitalWrite(mixerMotorRelayPin, state ? HIGH : LOW);
}

void operateAuger(bool state) {
    digitalWrite(motor1DirectionPin, state ? HIGH : LOW); // Assumes HIGH to run motor
}

void operateAgitator(bool state) {
    digitalWrite(motor2DirectionPin, state ? HIGH : LOW); // Assumes HIGH to run motor
}

void mixForDuration(unsigned long duration) {
    unsigned long startTime = millis();
    while(millis() - startTime < duration) {
        operateMixer(true);
    }
    operateMixer(false);
}
