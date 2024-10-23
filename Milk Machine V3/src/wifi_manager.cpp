#include "wifi_manager.h"
#include "lcd_manager.h"
#include "config.h"

void setupWiFi() {
    displayMessage("Configuring WiFi");

    // Static IP setup (optional)
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        displayMessage("Static IP Failed");
        Serial.println("Failed to configure static IP, proceeding with DHCP.");
        delay(2000);
    }

    WiFi.begin(ssid, password);  // Start the WiFi connection
    displayMessage("Connecting WiFi");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        displayDot();  // Show loading dots on the LCD
        attempts++;
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        displayMessage("WiFi Connected");
        displayIPAddress(WiFi.localIP());
        Serial.println("\nWiFi connected successfully.");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        displayMessage("WiFi Failed");
        displayMessage("Check Settings");
        Serial.println("\nFailed to connect to WiFi.");
    }

    delay(1000);
    displayMessage("Starting...");
}

void ensureWiFiConnection() {
    // Check if WiFi is still connected, and attempt reconnection if it's not
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Attempting to reconnect...");
        setupWiFi();  // Retry connection setup
    }
}
