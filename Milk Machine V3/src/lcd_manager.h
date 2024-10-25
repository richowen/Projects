// lcd_manager.h
#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <Arduino.h>
#include "config.h"

// Initialize LCD functions
void setupLCD();

// Update the LCD based on the current state
void updateLCD(State state, const char* errorMessage = nullptr);

// Clear the display
void clearLCD();

// Set the cursor position on the LCD
void setCursor(int col, int row);

// Print messages on the LCD
void printLCD(const char* message);
void printLCD(int value);

// Display the hopper level as a progress bar
void displayHopperLevel(int level);

// Display messages and status
void displayMessage(const char* message);
void displayMessageLine2(const char* message);
void displayErrorMessage(const char* errorMessage);

// Set the backlight color (RGB)
void setLCDColor(int r, int g, int b);

// Display the IP address
void displayIPAddress(IPAddress ip);

// Display a loading dot (for connecting animations)
void displayDot();

#endif // LCD_MANAGER_H
