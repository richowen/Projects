#include "laser_sensor.h"

LaserSensor::LaserSensor() : lastStableReading(0), lastStableTime(0) {}

bool LaserSensor::begin() {
    Wire.begin();
    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    byte error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("Laser sensor initialized successfully.");
        return true;
    } else {
        Serial.println("Failed to initialize laser sensor.");
        return false;
    }
}

uint8_t LaserSensor::readReg(uint8_t reg, uint8_t* buffer, size_t size) {
    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return 0; // Error
    }
    delay(20);
    Wire.requestFrom(LASER_SENSOR_ADDRESS, (uint8_t)size);
    for (size_t i = 0; i < size; i++) {
        buffer[i] = Wire.read();
    }
    return size;
}

bool LaserSensor::writeReg(uint8_t reg, const uint8_t* data, size_t size) {
    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    Wire.write(reg);
    for (size_t i = 0; i < size; i++) {
        Wire.write(data[i]);
    }
    return (Wire.endTransmission() == 0);
}

int LaserSensor::readHopperLevel() {
    uint8_t buf[2] = {0};
    uint8_t requestDistanceCmd = 0xB0;

    // Request distance data from sensor
    if (!writeReg(0x10, &requestDistanceCmd, 1)) {
        Serial.println("Error writing to laser sensor");
        return -1;
    }

    delay(50); // Small delay for sensor response

    // Read sensor data
    if (readReg(0x02, buf, 2) != 2) {
        Serial.println("Error reading from laser sensor");
        return -1;
    }

    // Convert raw data into distance
    int currentReading = buf[0] * 0x100 + buf[1] + 10;

    // Debouncing logic
    if (abs(currentReading - lastStableReading) > 5) {
        if (millis() - lastStableTime >= debounceDelay) {
            lastStableReading = currentReading;
            lastStableTime = millis();
        }
    } else {
        lastStableTime = millis(); // Update time but don't change reading
    }

    // Map distance to hopper level percentage
    int hopperLevel = map(lastStableReading, HOPPER_FULL_DISTANCE, HOPPER_EMPTY_THRESHOLD, 100, 0);
    return constrain(hopperLevel, 0, 100);
}

bool LaserSensor::isHopperLow() {
    return readHopperLevel() < 10; // Hopper is considered low if below 10%
}
