// debug_utils.h
#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include "water_level_sensor.h"
#include "error_handler.h"

// Constants for Telnet
extern const uint16_t TELNET_PORT;
extern const uint8_t MAX_TELNET_CLIENTS;
extern const unsigned long TELNET_TIMEOUT;

extern State currentState;
extern int hopperLevel;
extern WaterLevelSensor waterSensor;
extern ErrorHandler* errorHandler;
extern const char* getStateString(State state);

// Global variables
extern WiFiServer telnetServer;
extern WiFiClient telnetClients[];
extern bool telnetEnabled;

// Debug print functions
void printDebugInfo();
void debugPrint(const char* message);
void debugPrint(String message);
void debugPrintln(const char* message);
void debugPrintln(String message);
void debugPrintf(const char* format, ...);

// Telnet functions
void setupTelnet();
void handleTelnet();

#endif // DEBUG_UTILS_H