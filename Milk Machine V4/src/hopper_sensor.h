// hopper_sensor.h
#ifndef HOPPER_SENSOR_H
#define HOPPER_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class HopperSensor
{
public:
    HopperSensor(uint8_t address = 0x74);
    void begin();
    int getDistance();   // Returns distance in mm
    int getPercentage(); // Returns level as percentage
    bool isLow();        // Returns true if hopper level is low

private:
    uint8_t _address;
    const int FULL_LEVEL_MM = 10;   // Distance reading when hopper is full
    const int EMPTY_LEVEL_MM = 500; // Distance reading when hopper is empty
    const int LOW_THRESHOLD = 10;   // Percentage threshold for low level warning

    uint8_t readReg(uint8_t reg, uint8_t *pBuf, size_t size);
    bool writeReg(uint8_t reg, const uint8_t *pBuf, size_t size);
};

// hopper_sensor.cpp
HopperSensor::HopperSensor(uint8_t address) : _address(address) {}

void HopperSensor::begin()
{
    Wire.begin();
}

int HopperSensor::getDistance()
{
    uint8_t buf[2] = {0};
    uint8_t dat = 0xB0;

    writeReg(0x10, &dat, 1);
    delay(50);
    readReg(0x02, buf, 2);

    return buf[0] * 0x100 + buf[1] + 10; // Convert to mm
}

int HopperSensor::getPercentage()
{
    int distance = getDistance();

    // Constrain distance to valid range
    distance = constrain(distance, FULL_LEVEL_MM, EMPTY_LEVEL_MM);

    // Convert to percentage (inverted - shorter distance means more powder)
    return map(distance, EMPTY_LEVEL_MM, FULL_LEVEL_MM, 0, 100);
}

bool HopperSensor::isLow()
{
    return getPercentage() < LOW_THRESHOLD;
}

uint8_t HopperSensor::readReg(uint8_t reg, uint8_t *pBuf, size_t size)
{
    if (pBuf == NULL)
    {
        Serial.println("pBuf ERROR!! : null pointer");
        return 0;
    }

    Wire.beginTransmission(_address);
    Wire.write(&reg, 1);
    if (Wire.endTransmission() != 0)
    {
        return 0;
    }

    delay(20);
    Wire.requestFrom(_address, (uint8_t)size);
    for (uint16_t i = 0; i < size; i++)
    {
        pBuf[i] = Wire.read();
    }
    return size;
}

bool HopperSensor::writeReg(uint8_t reg, const uint8_t *pBuf, size_t size)
{
    if (pBuf == NULL)
    {
        Serial.println("pBuf ERROR!! : null pointer");
        return false;
    }

    Wire.beginTransmission(_address);
    Wire.write(&reg, 1);
    for (uint16_t i = 0; i < size; i++)
    {
        Wire.write(pBuf[i]);
    }
    return Wire.endTransmission() == 0;
}

#endif // HOPPER_SENSOR_H