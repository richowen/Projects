// lcd_manager.h
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include "DFRobot_RGBLCD1602.h"

class LCDManager {
public:
    LCDManager(uint8_t address = 0x2D) : lcd(address, 16, 2) {}
    
    void begin() {
        lcd.init();
        setColor(0, 255, 0);  // Default green
        lcd.clear();
    }
    
    // Update the main display with state and hopper level
    void updateDisplay(const char* state, int hopperLevel) {
        // Only update if content would change
        char newTopLine[17];
        char newBottomLine[17];
        snprintf(newTopLine, sizeof(newTopLine), "State: %-9s", state);
        snprintf(newBottomLine, sizeof(newBottomLine), "Hopper: %d%%    ", hopperLevel);
        
        if (strcmp(newTopLine, currentTopLine) != 0 || strcmp(newBottomLine, currentBottomLine) != 0) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(newTopLine);
            lcd.setCursor(0, 1);
            lcd.print(newBottomLine);
            
            strncpy(currentTopLine, newTopLine, sizeof(currentTopLine));
            strncpy(currentBottomLine, newBottomLine, sizeof(currentBottomLine));
        }
    }
    
    // Display error message with instruction
    void showError(const char* message) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(message);
        lcd.setCursor(0, 1);
        lcd.print("Toggle WashSwtch");
        
        strncpy(currentTopLine, message, sizeof(currentTopLine));
        strncpy(currentBottomLine, "Toggle WashSwtch", sizeof(currentBottomLine));
    }
    
    void setStateColor(const char* state) {
        if (strcmp(state, "IDLE") == 0) {
            setColor(0, 255, 0);  // Green
        } else if (strcmp(state, "MIXING") == 0) {
            setColor(0, 0, 255);  // Blue
        } else if (strcmp(state, "POST-MIX") == 0) {
            setColor(0, 255, 255);  // Cyan
        } else if (strcmp(state, "WASH") == 0) {
            setColor(255, 165, 0);  // Orange
        } else if (strcmp(state, "ERROR") == 0) {
            setColor(255, 0, 0);  // Red
        }
    }

private:
    DFRobot_RGBLCD1602 lcd;
    char currentTopLine[17] = "";  // Store current display content
    char currentBottomLine[17] = "";
    
    void setColor(int r, int g, int b) {
        lcd.setRGB(r, g, b);
    }
};

#endif // LCD_MANAGER_H