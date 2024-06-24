#include <MyFunctions.h>  // Include the header file

void setup() {
  // Initialize debug serial first for early debugging.
  Serial.begin(115200);
  Serial1.begin(115200);

  // Initialize other Serial and I2C
  mySerial.begin(9600);
  
  Wire.begin();

  // Initialize other pins and devices
  initializePins();

  lcd.init();
  
  // Initialize sensor time (or move this to initializePins if it makes more sense there)
  sensorHighStartTime = millis(); // Initialize to current millis

  // Try to initialize non-essential services, and report on their statuses
    (!initIOT());
    (!initWiFi());
    (!initHTTP());
    (!initIFTTT());

}

void loop() {
  // Update the debouncing states for all switches
  debouncer.update();
  debouncerWash.update();
  debouncerWater.update();
  
  // Handle the water relay based on the water switch state
  handleWaterSwitch();
  
  // Handle the washing state and switch the system to washing mode if needed
  handleWashFunction();
  
  // Update the LCD display irrespective of washing mode
  updateLCD(lcd, isWashing);

  // If the system is in washing mode, we don't need to proceed further
  // All tasks below this point are irrelevant during washing mode
  if(isWashing) {
    return;  // Exit loop early
  }
  
  // If not in washing mode, proceed with regular operation
  manageOperation();
}
