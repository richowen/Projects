#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "config.h"
#include "state_machine.h"
#include "logger.h"
#include <DFRobot_RGBLCD1602.h>

class LCDDisplay : public LogObserver {
private:
    Logger& _logger;
    DFRobot_RGBLCD1602 _lcd;
    SystemState _currentState = SystemState::IDLE;

public:
    LCDDisplay(Logger& logger) : _logger(logger), _lcd(16, 2) {
        _logger.addObserver(this);
    }

    ~LCDDisplay() {
        _logger.removeObserver(this);
    }

    void initialize() {
        _logger.info("Initializing LCD display...");
        _lcd.init();
        _lcd.setRGB(0, 255, 0);
        _lcd.print("Milk Machine V5");
        _lcd.setCursor(0, 1);
        _lcd.print("Initializing...");
        _logger.info("LCD display initialized.");
    }

    void update(SystemState state) {
        if (state == _currentState) return;

        _currentState = state;
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("State: ");

        const char* stateStr;
        uint8_t r, g, b;

        switch (state) {
            case SystemState::IDLE:
                stateStr = "IDLE";
                r = 0; g = 255; b = 0; // Green
                break;
            case SystemState::MIXING:
                stateStr = "MIXING";
                r = 255; g = 165; b = 0; // Orange
                break;
            case SystemState::POST_MIX:
                stateStr = "POST_MIX";
                r = 255; g = 165; b = 0; // Orange
                break;
            case SystemState::PERIODIC_MIX:
                stateStr = "PERIODIC";
                r = 255; g = 165; b = 0; // Orange
                break;
            case SystemState::WASH_STANDBY:
                stateStr = "WASH";
                r = 0; g = 0; b = 255; // Blue
                break;
            case SystemState::FAULT:
                stateStr = "FAULT";
                r = 255; g = 0; b = 0; // Red
                break;
        }

        _lcd.print(stateStr);
        _lcd.setRGB(r, g, b);
    }

    void onLogMessage(const String& message) override {
        // Could display error messages on LCD if needed
    }
};

#endif // LCD_DISPLAY_H