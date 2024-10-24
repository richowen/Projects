#include "water_level_sensor.h"

WaterLevelSensor::WaterLevelSensor() : _readIndex(0), _readCount(0), _currentLevel(0), _errorCount(0), _lastReadTime(0) {
    memset(_lowData, 0, sizeof(_lowData));
    memset(_highData, 0, sizeof(_highData));
}

void WaterLevelSensor::begin() {
    Wire.begin();  // Initialize I2C
    
    // Test communication with both ATtiny chips
    Wire.beginTransmission(ATTINY1_HIGH_ADDR);
    bool highSectionOk = (Wire.endTransmission() == 0);
    
    Wire.beginTransmission(ATTINY2_LOW_ADDR);
    bool lowSectionOk = (Wire.endTransmission() == 0);
    
    if (!highSectionOk || !lowSectionOk) {
        Serial.println("WARNING: Water sensor initialization issue!");
        if (!highSectionOk) Serial.println("- High section not responding");
        if (!lowSectionOk) Serial.println("- Low section not responding");
    }
}

bool WaterLevelSensor::update() {
    if (millis() - _lastReadTime < READ_INTERVAL) {
        return true;
    }

    bool success = true;
    success &= getLowSectionValue();
    success &= getHighSectionValue();

    if (success) {
        uint8_t newLevel = calculateWaterLevelSequential();
        
        // Implement simple spike filter
        if (_readCount > 0 && abs(newLevel - _currentLevel) > 20) {
            Serial.printf("Large level change detected: %d -> %d\n", _currentLevel, newLevel);
        }
        
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
        if (_errorCount >= MAX_ERRORS) {
            Serial.println("WARNING: Water sensor read errors exceeded threshold");
        }
    }

    return success;
}

uint8_t WaterLevelSensor::getLevel() const {
    return _currentLevel;
}

bool WaterLevelSensor::isReliable() const {
    return _errorCount < MAX_ERRORS && _readCount >= MIN_RELIABLE_READING;
}

bool WaterLevelSensor::getLowSectionValue() {
    Wire.requestFrom(ATTINY2_LOW_ADDR, (uint8_t)8, (uint8_t)true);  // Request 8 bytes from low section
    if (Wire.available() != 8) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        _lowData[i] = Wire.read();
    }
    return true;
}

bool WaterLevelSensor::getHighSectionValue() {
    Wire.requestFrom(ATTINY1_HIGH_ADDR, (uint8_t)12, (uint8_t)true);  // Request 12 bytes from high section
    if (Wire.available() != 12) {
        return false;
    }
    for (int i = 0; i < 12; i++) {
        _highData[i] = Wire.read();
    }
    return true;
}

uint8_t WaterLevelSensor::calculateWaterLevelSequential() {
    bool states[20];
    memset(states, 0, sizeof(states));

    // Check wet/dry states for low and high sections
    for (int i = 0; i < 8; i++) {
        states[i] = _lowData[i] > THRESHOLD;
    }
    for (int i = 0; i < 12; i++) {
        states[i + 8] = _highData[i] > THRESHOLD;
    }

    int wetCount = 0;
    for (int i = 0; i < 20; i++) {
        if (!states[i]) break;  // Stop counting at the first dry pad
        wetCount++;
    }

    return wetCount * 5;  // Each pad represents 5%
}

bool WaterLevelSensor::isSequentialReading() {
    bool foundGap = false;
    bool states[20];
    memset(states, 0, sizeof(states));

    // Combine low and high section wet/dry states
    for (int i = 0; i < 8; i++) {
        states[i] = _lowData[i] > THRESHOLD;
    }
    for (int i = 0; i < 12; i++) {
        states[i + 8] = _highData[i] > THRESHOLD;
    }

    // Look for gaps between wet pads
    bool foundDry = false;
    for (int i = 0; i < 20; i++) {
        if (!states[i]) {
            foundDry = true;
        } else if (foundDry) {
            return false;  // Wet pad found after a dry one
        }
    }

    return true;
}

void WaterLevelSensor::printDebug() {
    Serial.println(F("Water sensor states (bottom to top):"));
    bool states[20];
    for (int i = 0; i < 8; i++) {
        states[i] = _lowData[i] > THRESHOLD;
    }
    for (int i = 0; i < 12; i++) {
        states[i + 8] = _highData[i] > THRESHOLD;
    }

    for (int i = 0; i < 20; i++) {
        Serial.print(i);
        Serial.print(": ");
        Serial.print(states[i] ? "WET" : "DRY");
        Serial.print(" (");
        Serial.print(i < 8 ? _lowData[i] : _highData[i-8]);
        Serial.println(")");
    }
    Serial.print(F("Calculated water level: "));
    Serial.print(_currentLevel);
    Serial.println(F("%"));
    if (!isSequentialReading()) {
        Serial.println(F("WARNING: Non-sequential reading detected!"));
    }
}
