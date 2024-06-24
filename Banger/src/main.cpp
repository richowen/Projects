#include <Arduino.h>

// Define the relay pin
const int RELAY_PIN = 12;

// Constants for timers
const int SOLENOID_ON_TIME_MILLIS = 5000;        // Time to keep the solenoid on (milliseconds)
const int WAIT_BETWEEN_ACTIVATIONS_MILLIS = 10000; // Wait time between activations (milliseconds)
const unsigned long MIN_LOOP_DELAY_MILLIS = 600000UL; // Minimum delay between loops (milliseconds) (10 minutes)
const unsigned long MAX_LOOP_DELAY_MILLIS = 1500000UL; // Maximum delay between loops (milliseconds) (25 minutes)

unsigned long previousMillis = 0;
unsigned long loopDelayMillis = 0;

void activateBirdScarer(int times);

void setup() {
  // Initialize the relay pin as an output
  pinMode(RELAY_PIN, OUTPUT);

  // Start with the relay off
  digitalWrite(RELAY_PIN, LOW);

  // Initialize serial communication at 9600 bits per second
  Serial.begin(9600);
  Serial.println("System Initialized");

  // Seed the random number generator
  randomSeed(analogRead(0));

  // Perform an initial activation cycle
  int initialRandomChoice = random(3);
  Serial.print("Initial Activation: ");
  Serial.print(initialRandomChoice + 1);
  Serial.println(" Time(s)");
  activateBirdScarer(initialRandomChoice + 1);

  // Initialize the first loop delay
  loopDelayMillis = random(MIN_LOOP_DELAY_MILLIS, MAX_LOOP_DELAY_MILLIS);
  previousMillis = millis(); // Update the previousMillis to the current time

  Serial.print("Next activation in ");
  Serial.print(loopDelayMillis / 60000);
  Serial.println(" minute(s)");
}

// Function to activate the bird scarer a specified number of times
void activateBirdScarer(int times) {
  for (int i = 0; i < times; i++) {
    // Turn the relay on (activate solenoid)
    digitalWrite(RELAY_PIN, HIGH);
    // Serial.println("Solenoid Activated"); // Uncomment if needed

    delay(SOLENOID_ON_TIME_MILLIS); // Keep the valve open for the specified time

    // Turn the relay off (deactivate solenoid)
    digitalWrite(RELAY_PIN, LOW);
    // Serial.println("Solenoid Deactivated"); // Uncomment if needed

    // Wait between activations if there are multiple activations
    if (i < times - 1) {
      delay(WAIT_BETWEEN_ACTIVATIONS_MILLIS);
    }
  }
}

void loop() {
  // Get the current time
  unsigned long currentMillis = millis();

  // Check if the specified time has passed
  if (currentMillis - previousMillis >= loopDelayMillis) {
    // Save the current time as the last activation time
    previousMillis = currentMillis;

    // Generate a random number between 0 and 2
    int randomChoice = random(3);

    // Call the function with the number of activations based on the random number
    Serial.print("Activating ");
    Serial.print(randomChoice + 1);
    Serial.println(" Time(s)");
    activateBirdScarer(randomChoice + 1);

    // Generate a new random delay for the next cycle
    loopDelayMillis = random(MIN_LOOP_DELAY_MILLIS, MAX_LOOP_DELAY_MILLIS);

    Serial.print("Next activation in ");
    Serial.print(loopDelayMillis / 60000);
    Serial.println(" minute(s)");
  }
}