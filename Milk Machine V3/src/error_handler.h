// error_handler.h
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "lcd_manager.h"
#include "water_level_sensor.h"

class ErrorHandler {
public:
    // Single error state
    static const uint8_t ERROR_NONE = 0x00;
    static const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x01;

    // Constructor taking WaterLevelSensor reference
    ErrorHandler(WaterLevelSensor& waterSensor) : _waterSensor(waterSensor) {
        begin();
    }
    
    void begin() {
        currentError = ERROR_NONE;
        errorStartTime = 0;
        lastErrorDisplay = 0;
    }

    // Added handle method to match main.cpp
    void handle(State currentState) {
        check(currentState);
        
        if (hasErrors() && (millis() - lastErrorDisplay >= ERROR_DISPLAY_INTERVAL)) {
            displayError();
            lastErrorDisplay = millis();
        }
    }

    bool hasErrors() const { 
        return currentError != ERROR_NONE; 
    }

    void check(State currentState) {
        if (currentState == MIXING && mixTimerActive) {
            if (millis() - mixStartTime >= maxMixingDuration) {
                currentError = ERROR_MIX_TIME_EXCEEDED;
                if (errorStartTime == 0) {
                    errorStartTime = millis();
                }
            }
        }
    }

    void handleErrorState() {
        // Update error display
        if (millis() - lastErrorDisplay >= ERROR_DISPLAY_INTERVAL) {
            displayError();
            lastErrorDisplay = millis();
        }
    }

private:
    WaterLevelSensor& _waterSensor;  // Store reference to water sensor
    uint8_t currentError;
    unsigned long errorStartTime;
    unsigned long lastErrorDisplay;
    
    static const unsigned long ERROR_DISPLAY_INTERVAL = 2000;

    void displayError() {
        clearLCD();
        setCursor(0, 0);
        printLCD("ERROR:");
        setCursor(0, 1);
        printLCD("Mix Time Exceeded");
        setCursor(0, 2);
        printLCD("Power Cycle Req");
    }
};

#endif // ERROR_HANDLER_H