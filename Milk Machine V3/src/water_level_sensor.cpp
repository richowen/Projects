// water_level_sensor.cpp
#include "water_level_sensor.h"

void WaterLevelSensor::begin() {
    Wire.begin();
    Wire.setClock(100000);  // 100kHz for stability
}

void WaterLevelSensor::getHigh12SectionValue() {
    memset(_highData, 0, sizeof(_highData));
    Wire.requestFrom(ATTINY1_HIGH_ADDR, 12);
    
    while (12 != Wire.available());
    
    for (int i = 0; i < 12; i++) {
        _highData[i] = Wire.read();
    }
    delay(10);
}

void WaterLevelSensor::getLow8SectionValue() {
    memset(_lowData, 0, sizeof(_lowData));
    Wire.requestFrom(ATTINY2_LOW_ADDR, 8);
    
    while (8 != Wire.available());
    
    for (int i = 0; i < 8; i++) {
        _lowData[i] = Wire.read();
    }
    delay(10);
}

void WaterLevelSensor::update() {
    uint32_t touch_val = 0;
    uint8_t trig_section = 0;
    
    getLow8SectionValue();
    getHigh12SectionValue();
    
    // Process low section data
    for (int i = 0; i < 8; i++) {
        if (_lowData[i] > THRESHOLD) {
            touch_val |= 1 << i;
        }
    }
    
    // Process high section data
    for (int i = 0; i < 12; i++) {
        if (_highData[i] > THRESHOLD) {
            touch_val |= (uint32_t)1 << (8 + i);
        }
    }
    
    // Calculate water level
    while (touch_val & 0x01) {
        trig_section++;
        touch_val >>= 1;
    }
    
    _waterLevel = trig_section * 5;
    
    // Basic reliability check - ensure readings are sequential
    bool foundDry = false;
    _isReliable = true;
    
    for (int i = 0; i < 8; i++) {
        if (_lowData[i] <= THRESHOLD) foundDry = true;
        else if (foundDry) {
            _isReliable = false;
            break;
        }
    }
    
    if (_isReliable) {
        for (int i = 0; i < 12; i++) {
            if (_highData[i] <= THRESHOLD) foundDry = true;
            else if (foundDry) {
                _isReliable = false;
                break;
            }
        }
    }
}

void WaterLevelSensor::printDebug() {
    Serial.println("Water Sensor Debug:");
    
    Serial.println("Low section values:");
    for (int i = 0; i < 8; i++) {
        Serial.print(_lowData[i]);
        Serial.print(" ");
    }
    Serial.println();
    
    Serial.println("High section values:");
    for (int i = 0; i < 12; i++) {
        Serial.print(_highData[i]);
        Serial.print(" ");
    }
    Serial.println();
    
    Serial.print("Water Level: ");
    Serial.print(_waterLevel);
    Serial.println("%");
    
    Serial.print("Reliable: ");
    Serial.println(_isReliable ? "Yes" : "No");
    Serial.println();
}