#include "water_level_sensor.h"
#include <Wire.h>

#define THRESHOLD           100
#define ATTINY1_HIGH_ADDR   0x78
#define ATTINY2_LOW_ADDR    0x77

uint8_t low_data[8] = {0};
uint8_t high_data[12] = {0};

WaterLevelSensor::WaterLevelSensor() : _currentLevel(0), _errorCount(0) {}

void WaterLevelSensor::begin() {
    Wire.begin();
    Wire.setClock(50000);  // Set clock to 50 kHz for stability with ATtiny chips

    // Check communication with both ATtiny chips
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

bool WaterLevelSensor::getLowSectionValue() {
    memset(_lowData, 0, sizeof(_lowData));
    Wire.requestFrom(ATTINY2_LOW_ADDR, (uint8_t)8);
    while (Wire.available() < 8);
    for (int i = 0; i < 8; i++) {
        _lowData[i] = Wire.read();
    }
    delay(10);
    return true;  // Return true to indicate a successful read
}

bool WaterLevelSensor::getHighSectionValue() {
    memset(_highData, 0, sizeof(_highData));
    Wire.requestFrom(ATTINY1_HIGH_ADDR, (uint8_t)12);
    while (Wire.available() < 12);
    for (int i = 0; i < 12; i++) {
        _highData[i] = Wire.read();
    }
    delay(10);
    return true;  // Return true to indicate a successful read
}

uint8_t WaterLevelSensor::calculateWaterLevel() {
    uint32_t touch_val = 0;
    uint8_t trig_section = 0;

    // Read low section data and calculate touch_val
    for (int i = 0; i < 8; i++) {
        if (low_data[i] > THRESHOLD) {
            touch_val |= 1 << i;
        }
    }

    // Read high section data and calculate touch_val
    for (int i = 0; i < 12; i++) {
        if (high_data[i] > THRESHOLD) {
            touch_val |= (uint32_t)1 << (8 + i);
        }
    }

    // Calculate the highest continuous water level section
    while (touch_val & 0x01) {
        trig_section++;
        touch_val >>= 1;
    }

    // Each section represents 5%, so multiply by 5
    return trig_section * 5;
}

bool WaterLevelSensor::update() {
    // Read sensor values
    getLowSectionValue();
    getHighSectionValue();

    // Calculate water level as a percentage
    _currentLevel = calculateWaterLevel();

    return true;
}

uint8_t WaterLevelSensor::getLevel() const {
    return _currentLevel;
}

void WaterLevelSensor::printDebug() {
    Serial.println("Water sensor states (bottom to top):");

    Serial.print("Low 8 sections value: ");
    for (int i = 0; i < 8; i++) {
        Serial.print(low_data[i]);
        Serial.print(low_data[i] > THRESHOLD ? " (WET)" : " (DRY)");
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("High 12 sections value: ");
    for (int i = 0; i < 12; i++) {
        Serial.print(high_data[i]);
        Serial.print(high_data[i] > THRESHOLD ? " (WET)" : " (DRY)");
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("Calculated water level: ");
    Serial.print(_currentLevel);
    Serial.println("%");
}
