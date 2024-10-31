// water_level_sensor.h
#ifndef WATER_LEVEL_SENSOR_H
#define WATER_LEVEL_SENSOR_H

#include <Arduino.h>
#include "config.h"  // Include for debounceDelay

class WaterLevelSensor {
public:
    // Simplified water level states for pressure switch
    enum Level {
        EMPTY,      // Switch open (LOW with pullup)
        FULL,       // Switch closed (HIGH)
        ERROR       // Reserved for sensor failure detection
    };

    WaterLevelSensor(uint8_t pressurePin) 
        : _pressurePin(pressurePin) {}
    
    void begin() {
        pinMode(_pressurePin, INPUT_PULLUP);
        _lastValidReading = millis();
    }
    
    Level update() {
        // Read pressure switch (LOW = empty, HIGH = full with pullup)
        bool isFull = (digitalRead(_pressurePin) == LOW);
        
        // Debounce readings using global debounceDelay
        if (isFull != _lastReading) {
            if (millis() - _lastChangeTime >= debounceDelay) {
                _lastReading = isFull;
                _lastChangeTime = millis();
                
                // Update current level
                _currentLevel = isFull ? FULL : EMPTY;
                
                // Debug output on state change
                Serial.printf("Water level changed to: %s\n", 
                    _currentLevel == FULL ? "FULL" : "EMPTY");
            }
        }
        
        return _currentLevel;
    }
    
    Level getLevel() const {
        return _currentLevel;
    }
    
    bool isReliable() const {
        return _currentLevel != ERROR;
    }
    
    bool isEmpty() const { return _currentLevel == EMPTY; }
    bool isFull() const { return _currentLevel == FULL; }
    bool hasError() const { return _currentLevel == ERROR; }
    
    // Convert level to percentage for compatibility
    uint8_t getLevelPercent() const {
        switch (_currentLevel) {
            case EMPTY: return 0;
            case FULL: return 100;
            default: return 0;  // ERROR state
        }
    }

private:
    const uint8_t _pressurePin;
    Level _currentLevel = EMPTY;
    bool _lastReading = false;
    unsigned long _lastChangeTime = 0;
    unsigned long _lastValidReading = 0;
};

#endif // WATER_LEVEL_SENSOR_H