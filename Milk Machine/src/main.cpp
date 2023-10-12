#include <Arduino.h>

// Pin assignments
const int LIQUID_LEVEL_SENSOR_PIN = 0;
const int WATER_RELAY_PIN = 1;
const int MIXER_RELAY_PIN = 2;
const int AUGER_MOTOR_PWM_PIN = 5;
const int AGITATOR_MOTOR_PWM_PIN = 6;
const int MOTOR1_DIRECTION_PIN1 = 8; // Direction control for Motor 1
const int MOTOR1_DIRECTION_PIN2 = 9; // Speed control for Motor 1
const int MOTOR2_DIRECTION_PIN1 = 10; // Direction control for Motor 2
const int MOTOR2_DIRECTION_PIN2 = 11; // Speed control for Motor 2

// Motor speeds (adjust as needed)
const int AUGER_MOTOR_SPEED = 255; // Full speed
const int AGITATOR_MOTOR_SPEED = 255; // Full speed

// Delay settings
const unsigned long LOOP_DELAY = 100; // Loop delay in milliseconds
const unsigned long MOTOR_ON_THRESHOLD = 30000; // Motor on threshold in milliseconds
const unsigned long MIXER_RUN_TIME = 5000; // Mixer run time in milliseconds (5 seconds)

unsigned long lastMotorStartTime = 0; // To track the last time motors were started
bool mixerRunning = false;
unsigned long mixerStartTime = 0;
unsigned long lastSensorReadingTime = 0; // To track the last time the sensor was read
bool isSensorHigh = false; // Flag to track whether the sensor is currently HIGH
unsigned long sensorHighStartTime = 0; // To track the time since the sensor became HIGH

void setup()
{
    Serial.begin(9600); // Initialize serial communication
    pinMode(LIQUID_LEVEL_SENSOR_PIN, INPUT);
    pinMode(AUGER_MOTOR_PWM_PIN, OUTPUT);
    pinMode(AGITATOR_MOTOR_PWM_PIN, OUTPUT);
    pinMode(WATER_RELAY_PIN, OUTPUT);
    pinMode(MIXER_RELAY_PIN, OUTPUT);
    pinMode(MOTOR1_DIRECTION_PIN1, OUTPUT); // Direction control for Motor 1
    pinMode(MOTOR1_DIRECTION_PIN2, OUTPUT); // Speed control for Motor 1
    pinMode(MOTOR2_DIRECTION_PIN1, OUTPUT); // Direction control for Motor 2
    pinMode(MOTOR2_DIRECTION_PIN2, OUTPUT); // Speed control for Motor 2
    digitalWrite(LIQUID_LEVEL_SENSOR_PIN, HIGH); // Initialize the sensor (if necessary)
    digitalWrite(MIXER_RELAY_PIN, LOW); // Ensure the mixer relay is initially off
}

void startMotors()
{
    // Set motor speeds
    analogWrite(AUGER_MOTOR_PWM_PIN, AUGER_MOTOR_SPEED);
    analogWrite(AGITATOR_MOTOR_PWM_PIN, AGITATOR_MOTOR_SPEED);
    digitalWrite(MOTOR1_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR1_DIRECTION_PIN2, LOW);
    digitalWrite(MOTOR2_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN2, LOW);
    lastMotorStartTime = millis(); // Record the start time of motors
}

void stopMotors()
{
    // Stop motors
    analogWrite(AUGER_MOTOR_PWM_PIN, 0);
    analogWrite(AGITATOR_MOTOR_PWM_PIN, 0);
    digitalWrite(MOTOR1_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR1_DIRECTION_PIN2, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN2, HIGH);
}

void turnOnWaterRelay()
{
    // Turn on the water relay
    digitalWrite(WATER_RELAY_PIN, HIGH);
}

void turnOffWaterRelay()
{
    // Turn off the water relay
    digitalWrite(WATER_RELAY_PIN, LOW);
}

void turnOnMixerRelay()
{
    // Turn on the mixer relay
    digitalWrite(MIXER_RELAY_PIN, HIGH);
    mixerRunning = true; // Set the mixer running flag
}

void turnOffMixerRelay()
{
    // Turn off the mixer relay
    digitalWrite(MIXER_RELAY_PIN, LOW);
    mixerRunning = false; // Reset the mixer running flag
}

void countSensorHighTime()
{
    unsigned long currentMillis = millis();
    int sensorState = digitalRead(LIQUID_LEVEL_SENSOR_PIN);

    if (sensorState == HIGH) {
        // Sensor is currently HIGH
        if (!isSensorHigh) {
            // First time it's HIGH, record the start time
            sensorHighStartTime = currentMillis;
            isSensorHigh = true;
        }
        else if (currentMillis - sensorHighStartTime >= MOTOR_ON_THRESHOLD) {
            // Sensor has been HIGH for more than the threshold
            // Output information to the serial monitor
            Serial.println("Sensor has been HIGH for more than 10 seconds. Turning everything off and stopping the loop");

            // Turn off motors and relays
            stopMotors();
            turnOffWaterRelay();
            turnOffMixerRelay();

            // Stop the loop and effectively reset the board
            while (1)
                ;
        }
    }
    else {
        // Sensor is LOW, reset the flag
        isSensorHigh = false;
    }
}

void loop()
{
    // Read the sensor state
    int sensorState = digitalRead(LIQUID_LEVEL_SENSOR_PIN);

    if (sensorState == HIGH) {
        // Milk level is low, start motors and turn on relays
        startMotors();
        turnOnWaterRelay();
        if (!mixerRunning) {
            turnOnMixerRelay();
        }

        // Count the time the sensor has been HIGH
        countSensorHighTime();
    }
    else {
        // Sensor is LOW, reset the flag
        isSensorHigh = false;

        // Stop motors and turn off relays
        stopMotors();
        turnOffWaterRelay();

        if (mixerRunning) {
            // Check if the mixer has been running for 5 seconds after motors and relays turned off
            if (millis() - lastMotorStartTime >= MIXER_RUN_TIME) {
                turnOffMixerRelay(); // Turn off the mixer relay after 5 seconds
            }
        }
    }

    // Add a delay to control loop execution speed
    delay(LOOP_DELAY);
}
