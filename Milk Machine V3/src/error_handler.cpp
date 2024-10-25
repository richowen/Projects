// error_handler.cpp
#include "error_handler.h"

ErrorHandler* errorHandler = nullptr;

ErrorHandler::ErrorHandler(LaserSensor& laserSensor, WaterLevelSensor& waterSensor)
    : _laserSensor(laserSensor)
    , _waterSensor(waterSensor)
    , currentErrors(ERROR_NONE)
    , lastErrors(ERROR_NONE)
    , errorAcknowledged(false)
    , errorStartTime(0)
    , mixStartTime(0)
    , lastErrorDisplay(0)
{
}

void ErrorHandler::begin() {
    currentErrors = ERROR_NONE;
    lastErrors = ERROR_NONE;
    errorAcknowledged = false;
    errorStartTime = 0;
    mixStartTime = 0;
    lastErrorDisplay = 0;
}

void ErrorHandler::check(State currentState) {
    uint8_t newErrors = ERROR_NONE;

    // Hopper level error check
    if (_laserSensor.isHopperLow()) {
        newErrors |= ERROR_HOPPER_LOW;
    }

    // Mix time exceeded check
    if (currentState == MIXING) {
        if (mixStartTime == 0) {
            mixStartTime = millis();
        }
        if ((millis() - mixStartTime) >= maxMixingDuration) {
            newErrors |= ERROR_MIX_TIME_EXCEEDED;
        }
    } else {
        mixStartTime = 0; // Reset mix timer when not mixing
    }

    // Water sensor error check
    if (_waterSensor.getLevel() == WaterLevelSensor::ERROR) {
        newErrors |= ERROR_WATER_SENSOR_FAILURE;
    }

    // Log error changes
    if (newErrors != lastErrors) {
        Serial.printf("Error status changed: 0x%02X -> 0x%02X\n", lastErrors, newErrors);
        if (newErrors != 0 && lastErrors == 0) {
            // New error occurred
            errorStartTime = millis();
            errorAcknowledged = false;
        }
        lastErrors = newErrors;
    }

    currentErrors = newErrors;
}

void ErrorHandler::handle(State currentState) {
    check(currentState);
}

void ErrorHandler::handleErrorState() {
    if (millis() - lastErrorDisplay >= ERROR_DISPLAY_INTERVAL) {
        displayError();
        lastErrorDisplay = millis();
    }

    if (canClearError()) {
        Serial.println("Error conditions cleared");
        clearErrors();
    }
}

bool ErrorHandler::canClearError() const {
    // Must be in error state for minimum time
    if (millis() - errorStartTime < ERROR_CLEAR_DELAY) {
        return false;
    }

    if (currentErrors & ERROR_HOPPER_LOW) {
        return !_laserSensor.isHopperLow();
    }
    else if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
        return (digitalRead(resetSwitchPin) == LOW);
    }
    else if (currentErrors & ERROR_WATER_SENSOR_FAILURE) {
        return (_waterSensor.getLevel() != WaterLevelSensor::ERROR);
    }

    return false;
}

void ErrorHandler::displayError() {
    clearLCD();
    setCursor(0, 0);
    printLCD("ERROR:");
    setCursor(0, 1);

    if (currentErrors & ERROR_HOPPER_LOW) {
        printLCD("Low Hopper");
    }
    else if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
        printLCD("Mix Time Exceeded");
    }
    else if (currentErrors & ERROR_WATER_SENSOR_FAILURE) {
        printLCD("Water Sensor Fail");
    }
}

bool ErrorHandler::isSystemOperational(State currentState) const {
    return (currentErrors == ERROR_NONE || 
            (currentErrors == ERROR_HOPPER_LOW && currentState == MIXING));
}

void ErrorHandler::clearErrors() {
    currentErrors = ERROR_NONE;
    lastErrors = ERROR_NONE;
    errorAcknowledged = false;
    mixStartTime = 0;
}