#include <Wire.h>

#include "laser_sensor.h"

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

int readHopperLevel() {
  uint8_t buf[2] = {
    0
  };
  uint8_t dat = 0xB0;

  writeReg(0x10, & dat, 1);
  delay(50);
  readReg(0x02, buf, 2);

  int distance = buf[0] * 0x100 + buf[1] + 10;

  // Convert distance to a percentage
  int level = map(distance, HOPPER_FULL_DISTANCE, HOPPER_EMPTY_THRESHOLD, 100, 0);
  return constrain(level, 0, 100);
}

bool isHopperLow() {
  return readHopperLevel() < 20; // Consider hopper low when below 20%
}