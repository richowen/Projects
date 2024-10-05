// laser_sensor.h
#ifndef LASER_SENSOR_H     // Include guard to prevent multiple inclusions of this header file
#define LASER_SENSOR_H

#include <Arduino.h>       // Include Arduino core library for basic functions
#include <Wire.h>          // Include Wire library for I2C communication

// Define the I2C address of the laser sensor
#define LASER_SENSOR_ADDRESS 0x74

// Define thresholds for hopper level detection
#define HOPPER_EMPTY_THRESHOLD 1000  // Distance threshold for determining if hopper is empty (adjust based on hopper size)
#define HOPPER_FULL_DISTANCE 50      // Distance threshold indicating that the hopper is full

// Function prototypes for laser sensor management
void setupLaserSensor();            // Function to initialize the laser sensor
int readHopperLevel();              // Function to read and return the current hopper level
bool isHopperLow();                 // Function to check if the hopper is below the empty threshold

#endif // LASER_SENSOR_H            // End of include guard
