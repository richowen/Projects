// lcd_manager.cpp

#include "lcd_manager.h"
#include <DFRobot_RGBLCD1602.h>

DFRobot_RGBLCD1602 lcd(0x2D, 16, 2);

void setupLCD() {
  lcd.init();
  lcd.setRGB(0, 255, 0);
  lcd.print("Milk Mixer Ready");
}

void updateLCD(State state) {
  lcd.clear();
  switch (state) {
    case IDLE:
      lcd.setRGB(0, 255, 0);
      lcd.print("READY");
      break;
    case WAITING_PRE_MIX:
      lcd.setRGB(255, 255, 0);
      lcd.print("WAITING");
      break;
    case MIXING:
      lcd.setRGB(0, 0, 255);
      lcd.print("MIXING");
      break;
    case WAITING_POST_MIX:
      lcd.setRGB(0, 255, 255);
      lcd.print("FINISHING");
      break;
    case ERROR:
      lcd.setRGB(255, 0, 0);
      lcd.print("ERROR: Mix time");
      lcd.setCursor(0, 1);
      lcd.print("exceeded!");
      break;
  }
}

void displayMessage(const char* message) {
  lcd.clear();
  lcd.print(message);
}

void displayDot() {
  lcd.print(".");
}

void displayIPAddress(IPAddress ip) {
  lcd.setCursor(0, 1);
  lcd.print(ip.toString());
}