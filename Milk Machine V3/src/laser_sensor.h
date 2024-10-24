#ifndef LASER_SENSOR_H
#define LASER_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

#define LASER_ADDRESS 0x74
#define HOPPER_EMPTY_THRESHOLD 1000  
#define HOPPER_FULL_DISTANCE 50      

class LaserSensor {
public:
    LaserSensor();
    bool begin();
    int readHopperLevel();
    bool isHopperLow();
    
private:
    bool writeReg(uint8_t reg, const void* pBuf, size_t size);
    uint8_t readReg(uint8_t reg, void* pBuf, size_t size);
    uint8_t buf[2];
    int lastReading;
};

#endif // LASER_SENSOR_H