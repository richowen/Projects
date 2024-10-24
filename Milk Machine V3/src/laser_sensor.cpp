#include "laser_sensor.h"

LaserSensor::LaserSensor() 
    : lastStableReading(0)
    , lastStableTime(0)
    , sensorInitialized(false) {}

bool LaserSensor::begin() {
    // Allow I2C bus to stabilize after power-on
    delay(100);
    
    Wire.begin();
    Wire.setClock(100000);  // Set to 100kHz for more reliable communication
    
    // Multiple initialization attempts
    for (int attempt = 0; attempt < I2C_INIT_RETRIES; attempt++) {
        Serial.printf("Laser sensor initialization attempt %d/%d\n", attempt + 1, I2C_INIT_RETRIES);
        
        resetSensor();
        
        if (testConnection()) {
            sensorInitialized = true;
            Serial.println("Laser sensor initialized successfully");
            return true;
        }
        
        delay(100 * (attempt + 1));  // Increasing delay between attempts
    }
    
    Serial.println("Failed to initialize laser sensor after multiple attempts");
    return false;
}

bool LaserSensor::testConnection() {
    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    if (Wire.endTransmission() == 0) {
        // Try reading a test value
        uint8_t testBuf[2] = {0};
        if (readReg(0x02, testBuf, 2) == 2) {
            return true;
        }
    }
    return false;
}

void LaserSensor::resetSensor() {
    // Perform soft reset if available on your sensor
    uint8_t resetCmd = 0x00;  // Replace with actual reset command
    writeReg(0x00, &resetCmd, 1);
    delay(50);  // Wait for reset to complete
}

uint8_t LaserSensor::readReg(uint8_t reg, uint8_t* buffer, size_t size) {
    if (!sensorInitialized) {
        Serial.println("Attempting to read from uninitialized sensor");
        return 0;
    }

    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {  // false to send restart condition
        Serial.println("Error during register selection");
        return 0;
    }
    
    unsigned long startTime = millis();
    Wire.requestFrom(LASER_SENSOR_ADDRESS, (uint8_t)size);
    
    // Wait with timeout
    while (Wire.available() < size) {
        if (millis() - startTime > I2C_TIMEOUT) {
            Serial.println("Timeout waiting for sensor response");
            return 0;
        }
    }
    
    for (size_t i = 0; i < size; i++) {
        buffer[i] = Wire.read();
    }
    
    return size;
}

bool LaserSensor::writeReg(uint8_t reg, const uint8_t* data, size_t size) {
    if (!sensorInitialized) {
        Serial.println("Attempting to write to uninitialized sensor");
        return false;
    }

    Wire.beginTransmission(LASER_SENSOR_ADDRESS);
    Wire.write(reg);
    for (size_t i = 0; i < size; i++) {
        Wire.write(data[i]);
    }
    
    byte error = Wire.endTransmission();
    if (error != 0) {
        Serial.printf("I2C write error: %d\n", error);
        return false;
    }
    
    return true;
}

int LaserSensor::readHopperLevel() {
    if (!sensorInitialized) {
        Serial.println("Cannot read hopper level - sensor not initialized");
        return -1;
    }

    uint8_t buf[2] = {0};
    uint8_t requestDistanceCmd = 0xB0;

    // Request distance measurement with retry
    for (int retry = 0; retry < 3; retry++) {
        if (writeReg(0x10, &requestDistanceCmd, 1)) {
            delay(50);  // Give sensor time to take measurement
            
            if (readReg(0x02, buf, 2) == 2) {
                // Convert raw data into distance
                int currentReading = buf[0] * 0x100 + buf[1] + 10;
                
                // Debouncing logic
                if (abs(currentReading - lastStableReading) > 5) {
                    if (millis() - lastStableTime >= debounceDelay) {
                        lastStableReading = currentReading;
                        lastStableTime = millis();
                    }
                } else {
                    lastStableTime = millis();
                }

                // Map distance to hopper level percentage
                int hopperLevel = map(lastStableReading, 
                                    HOPPER_FULL_DISTANCE, 
                                    HOPPER_EMPTY_THRESHOLD, 
                                    100, 0);
                return constrain(hopperLevel, 0, 100);
            }
        }
        delay(50);  // Wait before retry
    }
    
    Serial.println("Failed to read hopper level after retries");
    return -1;
}

bool LaserSensor::isHopperLow() {
    int level = readHopperLevel();
    return (level >= 0 && level < 10);
}