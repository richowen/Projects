#include "laser_sensor.h"

LaserSensor::LaserSensor() : lastReading(0) {
    memset(buf, 0, sizeof(buf));
}

bool LaserSensor::begin() {
    Wire.begin();
    Wire.beginTransmission(LASER_ADDRESS);
    return (Wire.endTransmission() == 0);
}

uint8_t LaserSensor::readReg(uint8_t reg, void* pBuf, size_t size) {
    if (pBuf == NULL) {
        Serial.println("pBuf ERROR!! : null pointer");
        return 0;
    }
    
    uint8_t* _pBuf = (uint8_t*)pBuf;
    
    Wire.beginTransmission(LASER_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return 0;
    }
    
    delay(20);
    Wire.requestFrom(LASER_ADDRESS, (uint8_t)size);
    
    for (uint16_t i = 0; i < size; i++) {
        _pBuf[i] = Wire.read();
    }
    
    return size;
}

bool LaserSensor::writeReg(uint8_t reg, const void* pBuf, size_t size) {
    if (pBuf == NULL) {
        Serial.println("pBuf ERROR!! : null pointer");
        return false;
    }
    
    const uint8_t* _pBuf = (const uint8_t*)pBuf;
    
    Wire.beginTransmission(LASER_ADDRESS);
    Wire.write(reg);
    for (uint16_t i = 0; i < size; i++) {
        Wire.write(_pBuf[i]);
    }
    
    return (Wire.endTransmission() == 0);
}

int LaserSensor::readHopperLevel() {
    uint8_t dat = 0xB0;
    
    if (!writeReg(0x10, &dat, 1)) {
        Serial.println("Error writing to laser sensor");
        return -1;
    }
    
    delay(50);
    
    if (readReg(0x02, buf, 2) != 2) {
        Serial.println("Error reading from laser sensor");
        return -1;
    }
    
    int distance = buf[0] * 0x100 + buf[1] + 10;
    lastReading = distance;
    
    // Map distance to hopper level percentage
    int hopperLevel = map(distance, 
                         HOPPER_FULL_DISTANCE, 
                         HOPPER_EMPTY_THRESHOLD, 
                         100, 0);
    return constrain(hopperLevel, 0, 100);
}

bool LaserSensor::isHopperLow() {
    int level = readHopperLevel();
    return (level >= 0 && level < 10);
}