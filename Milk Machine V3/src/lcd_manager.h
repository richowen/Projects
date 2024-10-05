// lcd_manager.h
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include "config.h"

void setupLCD();
void updateLCD(State state, int hopperLevel, const char* errorMessage = nullptr);
void displayMessage(const char* message);
void displayMessageLine2(const char* message);
void clearLCD();
void setCursor(int col, int row);
void printLCD(const char* message);
void printLCD(int value);
void displayIPAddress(IPAddress ip);
void displayDot();
void setLCDColor(int r, int g, int b);
void displayHopperLevel(int level);

#endif // LCD_MANAGER_H