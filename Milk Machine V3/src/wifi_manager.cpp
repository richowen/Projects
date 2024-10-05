// wifi_manager.cpp

#include "wifi_manager.h"         // Include header file for WiFi management functions
#include "config.h"               // Include configuration settings
#include "lcd_manager.h"          // Include LCD management functions

void setupWiFi() {
  displayMessage("Configuring WiFi");  // Display message indicating WiFi setup is starting

  // Configure WiFi with a static IP address and network settings
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    displayMessage("Static IP Failed");  // Display error message if static IP configuration fails
    delay(2000);                          // Wait for 2 seconds before continuing
  }

  WiFi.begin(ssid, password);            // Begin the WiFi connection using SSID and password
  displayMessage("Connecting WiFi");     // Display message indicating WiFi is connecting

  int attempts = 0;                      // Initialize connection attempts counter
  // Attempt to connect to WiFi, allowing up to 20 attempts
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);                          // Wait half a second between attempts
    displayDot();                        // Display a dot on the LCD to indicate connection progress
    attempts++;                          // Increment the connection attempts counter
  }

  // Check if the connection was successful
  if (WiFi.status() == WL_CONNECTED) {
    displayMessage("WiFi Connected");    // Display message indicating successful connection
    displayIPAddress(WiFi.localIP());    // Display the assigned local IP address
  } else {
    displayMessage("WiFi Failed");        // Display error message if connection failed
    displayMessage("Check Settings");     // Prompt user to check WiFi settings
  }
  delay(2000);                            // Wait for 2 seconds before continuing
  displayMessage("Starting...");          // Indicate that the application is starting
  delay(1000);                            // Wait for 1 second before proceeding
}
