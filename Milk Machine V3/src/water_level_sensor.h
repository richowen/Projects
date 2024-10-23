// water_level_sensor.h
#ifndef WATER_LEVEL_SENSOR_H
#define WATER_LEVEL_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class WaterLevelSensor {
public:
    // Define constants as uint8_t to match Wire library expectations
    static const uint8_t ATTINY1_HIGH_ADDR = 0x78;
    static const uint8_t ATTINY2_LOW_ADDR = 0x77;
    static const uint8_t THRESHOLD = 100;
    static const uint8_t MIN_RELIABLE_READING = 3;

    WaterLevelSensor() {
        memset(_lowData, 0, sizeof(_lowData));
        memset(_highData, 0, sizeof(_highData));
    }

    void begin() {
        Wire.begin();
    }

    bool update() {
        if (millis() - _lastReadTime < READ_INTERVAL) {
            return true;
        }

        bool success = true;
        success &= getLowSectionValue();
        success &= getHighSectionValue();

        if (success) {
            uint8_t newLevel = calculateWaterLevelSequential();
            
            _readings[_readIndex] = newLevel;
            _readIndex = (_readIndex + 1) % MIN_RELIABLE_READING;
            
            if (++_readCount >= MIN_RELIABLE_READING) {
                uint16_t sum = 0;
                for (int i = 0; i < MIN_RELIABLE_READING; i++) {
                    sum += _readings[i];
                }
                _currentLevel = sum / MIN_RELIABLE_READING;
            }
            
            _lastReadTime = millis();
            _errorCount = 0;
        } else {
            _errorCount++;
        }

        return success;
    }

    uint8_t getLevel() const {
        return _currentLevel;
    }

    bool isReliable() const {
        return _errorCount < MAX_ERRORS && _readCount >= MIN_RELIABLE_READING;
    }

    void getPadStates(bool states[], int size) {
        if (size < 20) return;
        
        for (int i = 0; i < 8; i++) {
            states[i] = _lowData[i] > THRESHOLD;
        }
        for (int i = 0; i < 12; i++) {
            states[i + 8] = _highData[i] > THRESHOLD;
        }
    }

    void printDebug() {
        Serial.println(F("Pad states (bottom to top):"));
        bool states[20];
        getPadStates(states, 20);
        
        for (int i = 0; i < 20; i++) {
            Serial.print(i);
            Serial.print(": ");
            Serial.print(states[i] ? "WET" : "dry");
            Serial.print(" (");
            Serial.print(i < 8 ? _lowData[i] : _highData[i-8]);
            Serial.println(")");
        }
        
        Serial.print(F("Level: "));
        Serial.print(_currentLevel);
        Serial.println(F("%"));
        
        if (!isSequentialReading()) {
            Serial.println(F("WARNING: Non-sequential reading detected!"));
        }
    }

private:
    static const unsigned long READ_INTERVAL = 100;
    static const uint8_t MAX_ERRORS = 3;

    uint8_t _lowData[8] = {0};
    uint8_t _highData[12] = {0};
    uint8_t _readings[MIN_RELIABLE_READING] = {0};
    uint8_t _readIndex = 0;
    uint8_t _readCount = 0;
    uint8_t _currentLevel = 0;
    uint8_t _errorCount = 0;
    unsigned long _lastReadTime = 0;

    bool getLowSectionValue() {
        // Fix ambiguous call by using explicit types
        Wire.requestFrom(static_cast<uint8_t>(ATTINY2_LOW_ADDR), 
                        static_cast<uint8_t>(8),
                        static_cast<uint8_t>(true));  // true for send stop
                        
        if (Wire.available() != 8) {
            return false;
        }
        
        for (int i = 0; i < 8; i++) {
            _lowData[i] = Wire.read();
        }
        return true;
    }

    bool getHighSectionValue() {
        // Fix ambiguous call by using explicit types
        Wire.requestFrom(static_cast<uint8_t>(ATTINY1_HIGH_ADDR),
                        static_cast<uint8_t>(12),
                        static_cast<uint8_t>(true));  // true for send stop
                        
        if (Wire.available() != 12) {
            return false;
        }
        
        for (int i = 0; i < 12; i++) {
            _highData[i] = Wire.read();
        }
        return true;
    }

    bool isSequentialReading() {
        bool foundGap = false;
        bool states[20];
        getPadStates(states, 20);
        
        // Find first wet pad
        int firstWet = -1;
        for (int i = 0; i < 20; i++) {
            if (states[i]) {
                firstWet = i;
                break;
            }
        }
        
        if (firstWet == -1) return true; // No wet pads is valid
        
        // Check for gaps
        bool foundDry = false;
        for (int i = firstWet; i < 20; i++) {
            if (!states[i]) {
                foundDry = true;
            } else if (foundDry) {
                return false; // Found wet after dry
            }
        }
        
        return true;
    }

    uint8_t calculateWaterLevelSequential() {
    bool states[20];
    getPadStates(states, 20);
    
    int wetCount = 0;
    for (int i = 0; i < 20; i++) {
        if (!states[i]) break; // Stop counting at the first dry pad
        wetCount++;
    }
    
    return wetCount * 5;  // Each pad represents 5%
}
};

#endif // WATER_LEVEL_SENSOR_H