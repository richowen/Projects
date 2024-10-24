// water_level_sensor.h
#ifndef WATER_LEVEL_SENSOR_H
#define WATER_LEVEL_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

#define NO_TOUCH 0xFE
#define THRESHOLD 100
#define ATTINY1_HIGH_ADDR 0x78
#define ATTINY2_LOW_ADDR 0x77

class WaterLevelSensor {
public:
    void begin();
    void update();
    uint8_t getLevel() const { return _waterLevel; }
    bool isReliable() const { return _isReliable; }
    void printDebug();

private:
    void getHigh12SectionValue();
    void getLow8SectionValue();
    
    uint8_t _highData[12] = {0};
    uint8_t _lowData[8] = {0};
    uint8_t _waterLevel = 0;
    bool _isReliable = false;
};

#endif // WATER_LEVEL_SENSOR_H