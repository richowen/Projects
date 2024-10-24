// laser_sensor.h
#ifndef LASER_SENSOR_H
#define LASER_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

#define LASER_SENSOR_ADDRESS 0x74
#define HOPPER_EMPTY_THRESHOLD 1000  
#define HOPPER_FULL_DISTANCE 50      
#define I2C_INIT_RETRIES 5
#define I2C_TIMEOUT 50  // milliseconds

class LaserSensor {
public:
    LaserSensor();
    bool begin();
    int readHopperLevel();
    bool isHopperLow();
    
private:
    bool writeReg(uint8_t reg, const uint8_t* data, size_t size);
    uint8_t readReg(uint8_t reg, uint8_t* buffer, size_t size);
    bool testConnection();
    void resetSensor();
    
    int lastStableReading;
    unsigned long lastStableTime;
    const unsigned long debounceDelay = 500;
    bool sensorInitialized;
};

#endif // LASER_SENSOR_H