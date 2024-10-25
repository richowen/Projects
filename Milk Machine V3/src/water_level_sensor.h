// water_level_sensor.h

#ifndef WATER_LEVEL_SENSOR_H
#define WATER_LEVEL_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class WaterLevelSensor {
public:
    WaterLevelSensor();
    
    // Initialize the sensor
    void begin();
    
    // Update the water level readings and return if the operation was successful
    bool update();
    
    // Get the current water level (0-100%)
    uint8_t getLevel() const;

    // Check if the readings are reliable
    bool isReliable() const;

    // Debugging function to print pad states and sensor details
    void printDebug();

private:
    // Helper functions to read from the sensor
    bool getLowSectionValue();
    bool getHighSectionValue();
    
    // Calculate the water level as a percentage
    uint8_t calculateWaterLevel();  // Updated to match implementation

    // Define sensor constants
    static const uint8_t ATTINY1_HIGH_ADDR = 0x78;
    static const uint8_t ATTINY2_LOW_ADDR = 0x77;
    static const uint8_t THRESHOLD = 100;
    static const uint8_t MIN_RELIABLE_READING = 3;
    static const unsigned long READ_INTERVAL = 100;
    static const uint8_t MAX_ERRORS = 3;

    // Sensor data arrays and tracking variables
    uint8_t _lowData[8];
    uint8_t _highData[12];
    uint8_t _readings[MIN_RELIABLE_READING];
    uint8_t _readIndex;
    uint8_t _readCount;
    uint8_t _currentLevel;
    uint8_t _errorCount;
    unsigned long _lastReadTime;
};

#endif // WATER_LEVEL_SENSOR_H
