#include "lcd_manager.h"                 // Include header file for LCD management functions
#include <Wire.h>                        // Include Wire library for I2C communication
#include <DFRobot_RGBLCD1602.h>          // Include library for RGB LCD module

// Create an instance of the RGB LCD with the specified address and dimensions
DFRobot_RGBLCD1602 lcd( /*RGBAddr*/ 0x2D, /*lcdCols*/ 16, /*lcdRows*/ 2);

void setupLCD() {
  Serial.println("Initializing LCD...");  // Print message to Serial Monitor
  lcd.init();                             // Initialize the LCD
  lcd.setRGB(0, 255, 0);                  // Set initial backlight color to green
  lcd.clear();                            // Clear the display
  lcd.print("Milk Mixer Ready");          // Display a welcome message
  Serial.println("LCD initialized");      // Confirm LCD initialization in Serial Monitor
}

void updateLCD(State state, int hopperLevel, const char * errorMessage) {
  lcd.clear();                            // Clear the LCD for fresh display
  switch (state) {
  case IDLE:
    lcd.setRGB(0, 255, 0);                // Set backlight color to green
    lcd.print("READY");                   // Display "READY" message
    break;
  case WAITING_PRE_MIX:
    lcd.setRGB(255, 255, 0);              // Set backlight color to yellow
    lcd.print("WAITING");                 // Display "WAITING" message
    break;
  case MIXING:
    lcd.setRGB(0, 0, 255);                // Set backlight color to blue
    lcd.print("MIXING");                  // Display "MIXING" message
    break;
  case WAITING_POST_MIX:
    lcd.setRGB(0, 255, 255);              // Set backlight color to cyan
    lcd.print("FINISHING");               // Display "FINISHING" message
    break;
  case ERROR:
    lcd.setRGB(255, 0, 0);                // Set backlight color to red
    lcd.print("ERROR: ");                 // Display "ERROR:" message
    if (errorMessage) {
      lcd.print(errorMessage);            // Print the specific error message if provided
    }
    break;
  case WASH_STANDBY:
    lcd.setRGB(255, 165, 0);              // Set backlight color to orange
    lcd.print("WASH STANDBY");            // Display "WASH STANDBY" message
    break;
  case WASH_DISPENSE:
    lcd.setRGB(0, 255, 255);              // Set backlight color to cyan
    lcd.print("WASH DISPENSE");           // Display "WASH DISPENSE" message
    break;
  }

  displayHopperLevel(hopperLevel);       // Update the display with the current hopper level
}

void displayHopperLevel(int level) {
  lcd.setCursor(0, 1);                   // Move cursor to the second line

  int barLength = map(level, 0, 100, 0, 16); // Map hopper level (0-100%) to bar length (0-16 characters)

  for (int i = 0; i < 16; i++) {         // Loop to create a visual representation of the hopper level
    if (i < barLength) {
      lcd.write(byte(255));              // Write a full block character for the filled part of the bar
    } else {
      lcd.write(' ');                    // Write a space for the empty part of the bar
    }
  }
}

void setLCDColor(int r, int g, int b) {
  lcd.setRGB(r, g, b);                   // Set the RGB color of the LCD backlight
}

void displayMessage(const char * message) {
  lcd.clear();                           // Clear the LCD before displaying the new message
  lcd.print(message);                    // Display the provided message on the LCD
}

void displayMessageLine2(const char * message) {
  lcd.setCursor(0, 1);                   // Move cursor to the second line
  lcd.print(message);                    // Display the provided message on the second line
}

void clearLCD() {
  lcd.clear();                           // Clear the entire LCD display
}

void setCursor(int col, int row) {
  lcd.setCursor(col, row);               // Set the cursor position on the LCD
}

void printLCD(const char * message) {
  lcd.print(message);                    // Print a string message to the LCD
}

void printLCD(int value) {
  lcd.print(value);                      // Print an integer value to the LCD
}

void displayIPAddress(IPAddress ip) {
  lcd.setCursor(0, 1);                   // Move cursor to the second line
  lcd.print(ip.toString());              // Display the IP address as a string
}

void displayDot() {
  lcd.print(".");                        // Print a dot on the LCD, often used for progress indication
}
