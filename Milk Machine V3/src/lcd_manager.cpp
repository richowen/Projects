#include "lcd_manager.h"

#include <Wire.h>

#include <DFRobot_RGBLCD1602.h>

// Make sure this address matches your LCD module
DFRobot_RGBLCD1602 lcd( /*RGBAddr*/ 0x2D, /*lcdCols*/ 16, /*lcdRows*/ 2);

void setupLCD() {
  Serial.println("Initializing LCD...");
  lcd.init();
  lcd.setRGB(0, 255, 0); // Set initial backlight color to green
  lcd.clear();
  lcd.print("Milk Mixer Ready");
  Serial.println("LCD initialized");
}

void updateLCD(State state, int hopperLevel,
  const char * errorMessage) {
  lcd.clear();
  switch (state) {
  case IDLE:
    lcd.setRGB(0, 255, 0); // Green
    lcd.print("READY");
    break;
  case WAITING_PRE_MIX:
    lcd.setRGB(255, 255, 0); // Yellow
    lcd.print("WAITING");
    break;
  case MIXING:
    lcd.setRGB(0, 0, 255); // Blue
    lcd.print("MIXING");
    break;
  case WAITING_POST_MIX:
    lcd.setRGB(0, 255, 255); // Cyan
    lcd.print("FINISHING");
    break;
  case ERROR:
    lcd.setRGB(255, 0, 0); // Red
    lcd.print("ERROR: ");
    if (errorMessage) {
      lcd.print(errorMessage);
    }
    break;
  case WASH_STANDBY:
    lcd.setRGB(255, 165, 0); // Orange
    lcd.print("WASH STANDBY");
    break;
  case WASH_DISPENSE:
    lcd.setRGB(0, 255, 255); // Cyan
    lcd.print("WASH DISPENSE");
    break;
  }

  displayHopperLevel(hopperLevel);
}

void displayHopperLevel(int level) {
  lcd.setCursor(0, 1);

  int barLength = map(level, 0, 100, 0, 16); // Map 0-100% to 0-16 characters

  for (int i = 0; i < 16; i++) {
    if (i < barLength) {
      lcd.write(byte(255)); // Full block character
    } else {
      lcd.write(' ');
    }
  }
}

void setLCDColor(int r, int g, int b) {
  lcd.setRGB(r, g, b);
}

void displayMessage(const char * message) {
  lcd.clear();
  lcd.print(message);
}

void displayMessageLine2(const char * message) {
  lcd.setCursor(0, 1);
  lcd.print(message);
}

void clearLCD() {
  lcd.clear();
}

void setCursor(int col, int row) {
  lcd.setCursor(col, row);
}

void printLCD(const char * message) {
  lcd.print(message);
}

void printLCD(int value) {
  lcd.print(value);
}

void displayIPAddress(IPAddress ip) {
  lcd.setCursor(0, 1);
  lcd.print(ip.toString());
}

void displayDot() {
  lcd.print(".");
}