// lcd_manager.h
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include "config.h"

void setupLCD();
void updateLCD(State state);
void displayMessage(const char* message);
void displayDot();
void displayIPAddress(IPAddress ip);

#endif // LCD_MANAGER_H