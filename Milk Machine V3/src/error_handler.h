// error_handler.h
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "lcd_manager.h"
#include "laser_sensor.h"
#include "water_level_sensor.h"

class ErrorHandler {
public:
    // Error flags
    static const uint8_t ERROR_NONE = 0x00;
    static const uint8_t ERROR_HOPPER_LOW = 0x01;
    static const uint8_t ERROR_MIX_TIME_EXCEEDED = 0x02;
    static const uint8_t ERROR_WATER_SENSOR_FAILURE = 0x03;

    ErrorHandler(LaserSensor& laserSensor, WaterLevelSensor& waterSensor);
    
    void begin();
    void check(State currentState);
    void handle(State currentState);
    void handleErrorState();
    bool isSystemOperational(State currentState) const;
    bool requiresErrorState() const { return hasErrors() && !inErrorState; }
    void setInErrorState(bool inError) { inErrorState = inError; }
    bool canExitErrorState() const { return !hasErrors() && inErrorState; }
    
    uint8_t getCurrentErrors() const { return currentErrors; }
    bool hasErrors() const { return currentErrors != ERROR_NONE; }
    
    // Error clearing methods
    void clearErrors();
    void acknowledgeErrors() { errorAcknowledged = true; }

private:
    LaserSensor& _laserSensor;
    WaterLevelSensor& _waterSensor;
    
    uint8_t currentErrors;
    uint8_t lastErrors;
    bool errorAcknowledged;
    unsigned long errorStartTime;
    unsigned long mixStartTime;
    unsigned long lastErrorDisplay;
    
    static const unsigned long ERROR_CLEAR_DELAY = 5000;    // Minimum time in error state
    static const unsigned long ERROR_DISPLAY_INTERVAL = 2000; // Time between error message updates
    
    bool canClearError() const;
    bool inErrorState = false;
    void displayError();
};

extern ErrorHandler* errorHandler; // Global error handler pointer

#endif // ERROR_HANDLER_H