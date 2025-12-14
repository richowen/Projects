#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Function prototypes
void initNetwork();
void setupWebRoutes();
void connectToWifi();
int8_t findStrongestAP();
void handleWebRequests();
void sendJsonResponse(AsyncWebServerRequest *request, const String& json);
void sendErrorResponse(AsyncWebServerRequest *request, int code, const String& message);

#endif // NETWORK_H