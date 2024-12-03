// lcd_manager.h
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include "DFRobot_RGBLCD1602.h"
#include "config.h"

class LCDManager {
public:
    LCDManager(uint8_t address) : lcd(address, LCD_COLS, LCD_ROWS) {}
    
    void begin() {
        lcd.init();
        setColor(COLOR_IDLE_R, COLOR_IDLE_G, COLOR_IDLE_B);  // Default green
        lcd.clear();
        Serial.println("LCD initialized!");
        
        // Test the display
        lcd.setCursor(0, 0);
        lcd.print("LCD Test");
        lcd.setCursor(0, 1);
        lcd.print("Initializing...");
        delay(1000);  // Show test message briefly
        lcd.clear();
    }
    
    // Update the main display with state
    void updateDisplay(const char* state) {
        char newTopLine[17];
        char newBottomLine[17];
        
        // Format top line with state
        snprintf(newTopLine, sizeof(newTopLine), "State: %-9s", state);
        snprintf(newBottomLine, sizeof(newBottomLine), "Ready          ");
        
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
            setColor(COLOR_IDLE_R, COLOR_IDLE_G, COLOR_IDLE_B);  // Green
        } else if (strcmp(state, "MIXING") == 0) {
            setColor(COLOR_MIXING_R, COLOR_MIXING_G, COLOR_MIXING_B);  // Blue
        } else if (strcmp(state, "POST-MIX") == 0) {
            setColor(COLOR_POST_MIX_R, COLOR_POST_MIX_G, COLOR_POST_MIX_B);  // Cyan
        } else if (strcmp(state, "WASH") == 0) {
            setColor(COLOR_WASH_R, COLOR_WASH_G, COLOR_WASH_B);  // Orange
        } else if (strcmp(state, "ERROR") == 0) {
            setColor(COLOR_ERROR_R, COLOR_ERROR_G, COLOR_ERROR_B);  // Red
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
