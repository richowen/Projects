// water_level_sensor.h
#ifndef WATER_LEVEL_SENSOR_H
#define WATER_LEVEL_SENSOR_H

#include <Arduino.h>

class WaterLevelSensor {
public:
    // Water level states based on float switch combinations
    enum Level {
        EMPTY,      // Both switches open
        PARTIAL,    // Bottom switch closed, top open
        FULL,       // Both switches closed
        ERROR       // Invalid state (top closed, bottom open)
    };

    WaterLevelSensor(uint8_t bottomPin, uint8_t topPin) 
        : _bottomPin(bottomPin), _topPin(topPin) {}
    
    void begin() {
        pinMode(_bottomPin, INPUT_PULLUP);
        pinMode(_topPin, INPUT_PULLUP);
        _lastValidReading = millis();
    }
    
    Level update() {
        // Read both switches (LOW = closed/wet, HIGH = open/dry due to pullup)
        bool bottomClosed = (digitalRead(_bottomPin) == LOW);
        bool topClosed = (digitalRead(_topPin) == LOW);
        
        // Debounce readings
        if (bottomClosed != _lastBottom || topClosed != _lastTop) {
            if (millis() - _lastChangeTime >= DEBOUNCE_TIME) {
                _lastBottom = bottomClosed;
                _lastTop = topClosed;
                _lastChangeTime = millis();
            }
        }
        
        // Determine water level state
        if (!_lastBottom && !_lastTop) {
            _currentLevel = EMPTY;
        } else if (_lastBottom && !_lastTop) {
            _currentLevel = PARTIAL;
        } else if (_lastBottom && _lastTop) {
            _currentLevel = FULL;
        } else {
            // Invalid state: top closed but bottom open
            _currentLevel = ERROR;
        }
        
        return _currentLevel;
    }
    
    Level getLevel() const {
        return _currentLevel;
    }
    
    bool isReliable() const {
        return _currentLevel != ERROR;
    }
    
    // Helper methods for state checking
    bool isEmpty() const { return _currentLevel == EMPTY; }
    bool isFull() const { return _currentLevel == FULL; }
    bool hasError() const { return _currentLevel == ERROR; }
    
    // Convert level to percentage for compatibility with existing code
    uint8_t getLevelPercent() const {
        switch (_currentLevel) {
            case EMPTY: return 0;
            case PARTIAL: return 50;
            case FULL: return 100;
            default: return 0;  // ERROR state
        }
    }

private:
    const uint8_t _bottomPin;
    const uint8_t _topPin;
    const unsigned long DEBOUNCE_TIME = 50;  // 50ms debounce
    
    Level _currentLevel = EMPTY;
    bool _lastBottom = false;
    bool _lastTop = false;
    unsigned long _lastChangeTime = 0;
    unsigned long _lastValidReading = 0;
};

#endif // WATER_LEVEL_SENSOR_H