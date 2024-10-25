// error_handler.h
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "lcd_manager.h"
#include "water_level_sensor.h"

class ErrorHandler {
public:
    // Error flags
    static const uint8_t ERROR_NONE = 0x00;
    static const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x01;
    static const uint8_t ERROR_WATER_SENSOR_FAILURE = 0x02;

    ErrorHandler(WaterLevelSensor& waterSensor) : _waterSensor(waterSensor) {
        begin();
    }
    
    void begin() {
        currentErrors = ERROR_NONE;
        lastErrors = ERROR_NONE;
        errorStartTime = 0;
        lastErrorDisplay = 0;
    }

    // Main error handling method called from loop()
    void handle(State currentState) {
        // First check for any new errors
        check(currentState);
        
        // Update error display if needed
        if (hasErrors() && (millis() - lastErrorDisplay >= ERROR_DISPLAY_INTERVAL)) {
            displayError();
            lastErrorDisplay = millis();
        }
    }

    void check(State currentState) {
        uint8_t newErrors = ERROR_NONE;

        // Mix time exceeded check
        if (currentState == MIXING || (currentState == ERROR && mixTimerActive)) {
            if (mixTimerActive && (millis() - mixStartTime >= maxMixingDuration)) {
                newErrors |= ERROR_MIX_TIME_EXCEEDED;
            }
        }

        // Water sensor error check
        if (_waterSensor.getLevel() == WaterLevelSensor::ERROR) {
            newErrors |= ERROR_WATER_SENSOR_FAILURE;
        }

        // Log error changes
        if (newErrors != lastErrors) {
            Serial.printf("Error status changed: 0x%02X -> 0x%02X\n", lastErrors, newErrors);
            if (newErrors != 0 && lastErrors == 0) {
                errorStartTime = millis();
            }
            lastErrors = newErrors;
        }

        currentErrors = newErrors;
    }

    // Method to handle the ERROR state
    void handleErrorState() {
        // Update error display
        if (millis() - lastErrorDisplay >= ERROR_DISPLAY_INTERVAL) {
            displayError();
            lastErrorDisplay = millis();
        }

        // Check if errors can be cleared
        if (canClearError()) {
            clearErrors();
        }
    }

    bool canClearError() const {
        // Must be in error state for minimum time
        if (millis() - errorStartTime < ERROR_CLEAR_DELAY) {
            return false;
        }

        if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
            // Cannot clear this error without power cycle
            return false;
        }
        else if (currentErrors & ERROR_WATER_SENSOR_FAILURE) {
            return (_waterSensor.getLevel() != WaterLevelSensor::ERROR);
        }

        return false;
    }

    void displayError() {
        clearLCD();
        setCursor(0, 0);
        printLCD("ERROR:");
        setCursor(0, 1);

        if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
            printLCD("Mix Time Exceeded");
            setCursor(0, 2);
            printLCD("Power Cycle Req");
        }
        else if (currentErrors & ERROR_WATER_SENSOR_FAILURE) {
            printLCD("Water Sensor Fail");
        }
    }

    uint8_t getCurrentErrors() const { return currentErrors; }
    bool hasErrors() const { return currentErrors != ERROR_NONE; }
    
    void clearErrors() {
        currentErrors = ERROR_NONE;
        lastErrors = ERROR_NONE;
        // Only reset mix timer if we're clearing the mix time exceeded error
        if (currentErrors & ERROR_MIX_TIME_EXCEEDED) {
            mixStartTime = 0;
            mixTimerActive = false;
        }
    }

private:
    WaterLevelSensor& _waterSensor;
    uint8_t currentErrors;
    uint8_t lastErrors;
    unsigned long errorStartTime;
    unsigned long lastErrorDisplay;
    
    static const unsigned long ERROR_CLEAR_DELAY = 5000;
    static const unsigned long ERROR_DISPLAY_INTERVAL = 2000;
};

extern ErrorHandler* errorHandler;

#endif // ERROR_HANDLER_H