// wifi_manager.cpp

#include "wifi_manager.h"
#include "config.h"
#include "lcd_manager.h"

void setupWiFi() {
  displayMessage("Configuring WiFi");
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    displayMessage("Static IP Failed");
    delay(2000);
  }

  WiFi.begin(ssid, password);
  displayMessage("Connecting WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    displayDot();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    displayMessage("WiFi Connected");
    displayIPAddress(WiFi.localIP());
  } else {
    displayMessage("WiFi Failed");
    displayMessage("Check Settings");
  }
  delay(2000);

delay(2000);
displayMessage("Starting...");
delay(1000);
}