#include <Bounce2.h>
#include <Arduino.h>
#include "Wire.h"

// Pin assignments using #define for memory efficiency
#define LIQUID_LEVEL_SENSOR_PIN 0
#define WATER_RELAY_PIN 1
#define MIXER_RELAY_PIN 12
#define AUGER_MOTOR_PWM_PIN 5
#define AGITATOR_MOTOR_PWM_PIN 6
#define MOTOR1_DIRECTION_PIN1 8
#define MOTOR1_DIRECTION_PIN2 9
#define MOTOR2_DIRECTION_PIN1 10
#define MOTOR2_DIRECTION_PIN2 11
#define WASH_PIN 13
#define address 0x74

// Motor speeds (adjust as needed)
#define AUGER_MOTOR_SPEED 255
#define AGITATOR_MOTOR_SPEED 255

// Delay settings
#define LOOP_DELAY 500
#define MOTOR_ON_THRESHOLD 5000
#define MIXER_RUN_TIME 5000
#define DISTANCE_THRESHOLD 600
#define DISTANCE_TIME_THRESHOLD 3000

Bounce debouncer = Bounce();  // Create a Bounce object
Bounce debouncerWash = Bounce();  // Create another Bounce object for wash pin


unsigned long previousMillis = 0; // will store last time the loop was updated
unsigned long lastMotorStartTime = 0;
bool mixerRunning = false;
unsigned long mixerStartTime = 0;
unsigned long lastSensorReadingTime = 0;
bool isSensorHigh = false;
unsigned long sensorHighStartTime = 0;
unsigned long distanceErrorStart = 0;
bool isWashing = false;
bool wasWashing = false;

void setup() {
    Serial.begin(9600);
    Wire.begin();
    pinMode(AUGER_MOTOR_PWM_PIN, OUTPUT);
    pinMode(AGITATOR_MOTOR_PWM_PIN, OUTPUT);
    pinMode(WATER_RELAY_PIN, OUTPUT);
    pinMode(MIXER_RELAY_PIN, OUTPUT);
    pinMode(MOTOR1_DIRECTION_PIN1, OUTPUT);
    pinMode(MOTOR1_DIRECTION_PIN2, OUTPUT);
    pinMode(MOTOR2_DIRECTION_PIN1, OUTPUT);
    pinMode(MOTOR2_DIRECTION_PIN2, OUTPUT);
    pinMode(WASH_PIN, INPUT);
    pinMode(WASH_PIN, INPUT_PULLUP);
    digitalWrite(LIQUID_LEVEL_SENSOR_PIN, LOW);

    debouncer.attach(LIQUID_LEVEL_SENSOR_PIN, INPUT);
    debouncer.interval(10);  // Set debounce interval to 5 ms
}

void startMotors() {
    analogWrite(AUGER_MOTOR_PWM_PIN, AUGER_MOTOR_SPEED);
    analogWrite(AGITATOR_MOTOR_PWM_PIN, AGITATOR_MOTOR_SPEED);
    digitalWrite(MOTOR1_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR1_DIRECTION_PIN2, LOW);
    digitalWrite(MOTOR2_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN2, LOW);
    lastMotorStartTime = millis();
}

void stopMotors() {
    analogWrite(AUGER_MOTOR_PWM_PIN, 0);
    analogWrite(AGITATOR_MOTOR_PWM_PIN, 0);
    digitalWrite(MOTOR1_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR1_DIRECTION_PIN2, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN1, HIGH);
    digitalWrite(MOTOR2_DIRECTION_PIN2, HIGH);
}

void turnOnWaterRelay() {
    digitalWrite(WATER_RELAY_PIN, HIGH);
}

void turnOffWaterRelay() {
    digitalWrite(WATER_RELAY_PIN, LOW);
}

void turnOnMixerRelay() {
    digitalWrite(MIXER_RELAY_PIN, HIGH);
    mixerRunning = true;
}

void turnOffMixerRelay() {
    digitalWrite(MIXER_RELAY_PIN, LOW);
    mixerRunning = false;
}

void countSensorHighTime() {
    unsigned long currentMillis = millis();
    int sensorState = debouncer.read();

    if (isWashing) {
        // If in wash mode, do not proceed with counting sensor high time
        return;
    }

    if (sensorState == HIGH) {
        if (!isSensorHigh) {
            sensorHighStartTime = currentMillis;
            isSensorHigh = true;
        } else if (currentMillis - sensorHighStartTime >= MOTOR_ON_THRESHOLD) {
            Serial.println("Sensor has been HIGH for more than " + String(MOTOR_ON_THRESHOLD / 1000) + " seconds. Turning everything off and stopping the loop");
            stopMotors();
            turnOffWaterRelay();
            turnOffMixerRelay();
            while (1);
        }
    } else {
        isSensorHigh = false;
    }
}

void handleWashFunction() {
    int washState = digitalRead(WASH_PIN);
    
    if (washState == LOW) {
        isWashing = true;
        // Turn off all motors and mixing relay
        turnOnWaterRelay();
        stopMotors();
        turnOffMixerRelay();
    } else {
        isWashing = false;
        if (wasWashing) {  // Check if it was previously in wash mode
            sensorHighStartTime = millis();  // Reset the sensorHighStartTime when exiting wash mode
        }
    }
    
    wasWashing = isWashing;  // Update wasWashing for the next iteration
}

uint8_t buf[2] = { 0 };
uint8_t dat = 0xB0;
int distance = 0;

uint8_t readReg(uint8_t reg, const void* pBuf, size_t size) {
  if (pBuf == NULL) {
    Serial.println("pBuf ERROR!! : null pointer");
  }
  uint8_t* _pBuf = (uint8_t*)pBuf;
  Wire.beginTransmission(address);
  Wire.write(&reg, 1);
  if (Wire.endTransmission() != 0) {
    return 0;
  }
  delay(20);
  Wire.requestFrom(address, (uint8_t)size);
  for (uint16_t i = 0; i < size; i++) {
    _pBuf[i] = Wire.read();
  }
  return size;
}

bool writeReg(uint8_t reg, const void* pBuf, size_t size) {
  if (pBuf == NULL) {
    Serial.println("pBuf ERROR!! : null pointer");
  }
  uint8_t* _pBuf = (uint8_t*)pBuf;
  Wire.beginTransmission(address);
  Wire.write(&reg, 1);

  for (uint16_t i = 0; i < size; i++) {
    Wire.write(_pBuf[i]);
  }
  if (Wire.endTransmission() != 0) {
    return 0;
  } else {
    return 1;
  }
}

void loop() {

    debouncer.update();  // Update the debouncer
    
    handleWashFunction();  // Call the handleWashFunction to manage washing state
    
    if (isWashing) {
        return;  // Exit the loop early if in washing mode
    }

    int sensorState = debouncer.read();

    if (sensorState == HIGH) {
        startMotors();
        turnOnWaterRelay();
        if (!mixerRunning) {
            turnOnMixerRelay();
        }
        countSensorHighTime();
    } else {
        isSensorHigh = false;
        stopMotors();
        turnOffWaterRelay();

        if (mixerRunning) {
            if (millis() - lastMotorStartTime >= MIXER_RUN_TIME) {
                turnOffMixerRelay();
            }
        }
    }

   //writeReg(0x10, &dat, 1);
    //delay(50);
    //readReg(0x02, buf, 2);
    //distance = buf[0] * 0x100 + buf[1] + 10;
    //Serial.print("distance=");
    //Serial.print(distance);
    //Serial.print("mm");
    //Serial.println("\t");
    //delay(100);

if(distance > DISTANCE_THRESHOLD) {
  if(distanceErrorStart == 0) {
    distanceErrorStart = millis(); // Record the start time when the distance exceeds the threshold
  }
  if(millis() - distanceErrorStart > DISTANCE_TIME_THRESHOLD) {
    Serial.println("Powder Low. Turning everything off and stopping the loop");
    stopMotors();
    turnOffWaterRelay();
    turnOffMixerRelay();
    while (1);
  }
} else {
  distanceErrorStart = 0; // Reset timer if distance is below the threshold
}
}
