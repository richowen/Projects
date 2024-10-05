// laser_sensor.h
#ifndef LASER_SENSOR_H
#define LASER_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

#define LASER_SENSOR_ADDRESS 0x74
#define HOPPER_EMPTY_THRESHOLD 1000 // Adjust this value based on your hopper size
#define HOPPER_FULL_DISTANCE 50    // Distance when hopper is full

void setupLaserSensor();
int readHopperLevel();
bool isHopperLow();

#endif // LASER_SENSOR_H