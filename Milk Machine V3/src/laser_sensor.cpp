#include <Wire.h>
#include "laser_sensor.h"
#include "config.h"

void setupLaserSensor() {
  Wire.begin(); // Ensure Wire is initialized here
  Wire.beginTransmission(LASER_SENSOR_ADDRESS);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println("Laser sensor found and initialized");
  } else {
    Serial.println("Laser sensor initialization failed");
  }
}

uint8_t readReg(uint8_t reg, uint8_t * pBuf, size_t size) {
  Wire.beginTransmission(LASER_SENSOR_ADDRESS);
  Wire.write( & reg, 1);
  if (Wire.endTransmission() != 0) {
    return 0;
  }
  delay(20);
  Wire.requestFrom(LASER_SENSOR_ADDRESS, (uint8_t) size);
  for (uint16_t i = 0; i < size; i++) {
    pBuf[i] = Wire.read();
  }
  return size;
}

bool writeReg(uint8_t reg,
  const uint8_t * pBuf, size_t size) {
  Wire.beginTransmission(LASER_SENSOR_ADDRESS);
  Wire.write( & reg, 1);
  for (uint16_t i = 0; i < size; i++) {
    Wire.write(pBuf[i]);
  }
  return (Wire.endTransmission() == 0);
}

// Function to read and debounce the laser sensor data
int readHopperLevel() {
  uint8_t buf[2] = {0};
  uint8_t dat = 0xB0;

  writeReg(0x10, &dat, 1);   // Write register to request distance data
  delay(50);                 // Small delay for sensor response
  readReg(0x02, buf, 2);     // Read the sensor data into buffer

  // Convert the raw data into distance
  int currentReading = buf[0] * 0x100 + buf[1] + 10;

  // Debouncing logic
  if (abs(currentReading - lastStableReading) > 5) { // Threshold for considering a significant change
    if (millis() - lastStableTime >= debounceDelay) {
      lastStableReading = currentReading;  // Accept the new stable reading
      lastStableTime = millis();           // Update the time of the stable reading
    }
  } else {
    // If the difference is small, just update the time without changing the stable reading
    lastStableTime = millis();
  }

  // Convert distance to a percentage based on HOPPER_FULL_DISTANCE and HOPPER_EMPTY_THRESHOLD
  int level = map(lastStableReading, HOPPER_FULL_DISTANCE, HOPPER_EMPTY_THRESHOLD, 100, 0);
  return constrain(level, 0, 100); // Return the debounced and constrained level
}

// Function to check if the hopper is low with debounced readings
bool isHopperLow() {
  return readHopperLevel() < 10; // Consider hopper low when below 10%
}