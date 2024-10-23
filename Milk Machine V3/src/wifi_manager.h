// wifi_manager.h

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

// Initialize the WiFi connection
void setupWiFi();

// Keep the WiFi connection alive and handle reconnection if necessary
void ensureWiFiConnection();

#endif // WIFI_MANAGER_H
