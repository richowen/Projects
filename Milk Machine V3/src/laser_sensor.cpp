#include <Wire.h>                     // Include Wire library for I2C communication
#include "laser_sensor.h"             // Include header file for laser sensor functions

void setupLaserSensor() {
  Wire.begin();                      // Initialize the I2C bus for communication
  Wire.beginTransmission(LASER_SENSOR_ADDRESS); // Start communication with the laser sensor
  byte error = Wire.endTransmission(); // End transmission and get the error status
  if (error == 0) {
    Serial.println("Laser sensor found and initialized"); // Confirm successful initialization
  } else {
    Serial.println("Laser sensor initialization failed");   // Report failure to initialize
  }
}

uint8_t readReg(uint8_t reg, uint8_t * pBuf, size_t size) {
  // Function to read from a specified register of the laser sensor
  Wire.beginTransmission(LASER_SENSOR_ADDRESS); // Begin transmission to the laser sensor
  Wire.write(&reg, 1);                          // Specify the register to read from
  if (Wire.endTransmission() != 0) {            // End transmission and check for errors
    return 0;                                    // Return 0 on error
  }
  delay(20);                                     // Short delay to allow the sensor to prepare the data
  Wire.requestFrom(LASER_SENSOR_ADDRESS, (uint8_t)size); // Request the specified number of bytes from the sensor
  for (uint16_t i = 0; i < size; i++) {         // Read the requested bytes
    pBuf[i] = Wire.read();                       // Store the read byte in the buffer
  }
  return size;                                   // Return the number of bytes read
}

bool writeReg(uint8_t reg, const uint8_t * pBuf, size_t size) {
  // Function to write to a specified register of the laser sensor
  Wire.beginTransmission(LASER_SENSOR_ADDRESS); // Begin transmission to the laser sensor
  Wire.write(&reg, 1);                          // Specify the register to write to
  for (uint16_t i = 0; i < size; i++) {         // Write the data to the sensor
    Wire.write(pBuf[i]);
  }
  return (Wire.endTransmission() == 0);         // Return true if transmission was successful
}

int readHopperLevel() {
  uint8_t buf[2] = {0};                          // Buffer to store the sensor reading
  uint8_t dat = 0xB0;                            // Command to request hopper level

  writeReg(0x10, &dat, 1);                       // Write the command to the sensor
  delay(50);                                     // Delay to allow sensor to process the command
  readReg(0x02, buf, 2);                         // Read the distance data from the sensor

  // Combine the two bytes into a single distance value
  int distance = buf[0] * 0x100 + buf[1] + 10;  // Calculate distance and add a small offset

  // Convert distance to a percentage of hopper level
  int level = map(distance, HOPPER_FULL_DISTANCE, HOPPER_EMPTY_THRESHOLD, 100, 0);
  return constrain(level, 0, 100);               // Constrain the level to be within 0 to 100 percent
}

bool isHopperLow() {
  // Check if the hopper is considered low (below 20% level)
  return readHopperLevel() < 20; 
}
