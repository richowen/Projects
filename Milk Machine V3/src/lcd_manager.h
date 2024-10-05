// lcd_manager.h
#ifndef LCD_MANAGER_H         // Include guard to prevent multiple inclusions of this header file
#define LCD_MANAGER_H

#include <Arduino.h>           // Include Arduino core library for basic functions
#include "config.h"            // Include configuration settings for the LCD

// Function prototypes for LCD management
void setupLCD();                                     // Initializes the LCD for use
void updateLCD(State state, int hopperLevel, const char* errorMessage = nullptr); // Updates the LCD with current state, hopper level, and an optional error message
void displayMessage(const char* message);            // Displays a message on the LCD
void displayMessageLine2(const char* message);       // Displays a message on the second line of the LCD
void clearLCD();                                     // Clears the entire LCD screen
void setCursor(int col, int row);                    // Sets the cursor position on the LCD
void printLCD(const char* message);                  // Prints a string message to the LCD
void printLCD(int value);                            // Prints an integer value to the LCD
void displayIPAddress(IPAddress ip);                 // Displays an IP address on the LCD
void displayDot();                                   // Displays a dot on the LCD, often used for progress indication
void setLCDColor(int r, int g, int b);               // Sets the color of the LCD backlight using RGB values
void displayHopperLevel(int level);                  // Displays the current hopper level percentage on the LCD

#endif // LCD_MANAGER_H          // End of include guard
