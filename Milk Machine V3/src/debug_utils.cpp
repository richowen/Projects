// debug_utils.cpp
#include "debug_utils.h"

// Constants
const uint16_t TELNET_PORT = 23;
const uint8_t MAX_TELNET_CLIENTS = 2;
const unsigned long TELNET_TIMEOUT = 1000;

// Global variables
WiFiServer telnetServer(TELNET_PORT);
WiFiClient telnetClients[MAX_TELNET_CLIENTS];
bool telnetEnabled = false;

// Debug print implementations
void debugPrint(const char* message) {
    Serial.print(message);
    if (telnetEnabled) {
        for (uint8_t i = 0; i < MAX_TELNET_CLIENTS; i++) {
            if (telnetClients[i] && telnetClients[i].connected()) {
                telnetClients[i].print(message);
            }
        }
    }
}

void debugPrint(String message) {
    debugPrint(message.c_str());
}

void debugPrintln(const char* message) {
    debugPrint(message);
    debugPrint("\r\n");
}

void debugPrintln(String message) {
    debugPrintln(message.c_str());
}

void debugPrintf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    // Only send output if it's within buffer size limits
    if (len > 0 && len < sizeof(buf)) {
        debugPrint(buf);
    }
}

// Telnet implementations
void setupTelnet() {
    telnetServer.begin();
    telnetServer.setNoDelay(true);
    telnetEnabled = true;
    debugPrintln("Telnet server started");
}

// In debug_utils.cpp, update the telnet connection handler and add error reporting:

void handleTelnet() {
    if (!telnetEnabled) return;

    // Check for new client connections
    if (telnetServer.hasClient()) {
        bool clientFound = false;
        WiFiClient serverClient = telnetServer.available();

        // Find first available slot
        for (uint8_t i = 0; i < MAX_TELNET_CLIENTS; i++) {
            if (!telnetClients[i] || !telnetClients[i].connected()) {
                if (telnetClients[i]) {
                    telnetClients[i].stop();  // Clean up any disconnected clients
                }
                telnetClients[i] = serverClient;
                debugPrintln("New telnet client connected");

                telnetClients[i].println("Welcome to Milk Mixer Debug Console");
                telnetClients[i].printf("System Uptime: %lu seconds\r\n", millis() / 1000);
                
                // Print initial system status
                printSystemStatus();
                
                clientFound = true;
                break;
            }
        }

        // Reject connection if no free slot is found
        if (!clientFound) {
            serverClient.println("Too many connections");
            serverClient.stop();
            debugPrintln("Telnet connection rejected - too many clients");
        }
    }

    // Handle data from connected clients
    for (uint8_t i = 0; i < MAX_TELNET_CLIENTS; i++) {
        if (telnetClients[i] && telnetClients[i].connected()) {
            // Check for input from telnet client
            while (telnetClients[i].available()) {
                char c = telnetClients[i].read();
                // Add command handling here if needed
                telnetClients[i].write(c);  // Echo back to client
            }
        } else if (telnetClients[i]) {
            telnetClients[i].stop();
        }
    }
}

// Add this helper function to print system status
void printSystemStatus() {
    debugPrintln("\n=== System Status ===");
    
    // Current state and uptime
    debugPrintf("State: %s\r\n", getStateString(currentState));
    debugPrintf("Uptime: %lu seconds\r\n", millis() / 1000);
    
    // Water level state
    const char* waterLevelStr;
    switch (waterSensor.getLevel()) {
        case WaterLevelSensor::EMPTY: waterLevelStr = "EMPTY"; break;
        case WaterLevelSensor::PARTIAL: waterLevelStr = "PARTIAL"; break;
        case WaterLevelSensor::FULL: waterLevelStr = "FULL"; break;
        default: waterLevelStr = "ERROR"; break;
    }
    debugPrintf("Water Level: %s\r\n", waterLevelStr);
    
    // Hopper level
    debugPrintf("Hopper Level: %d%%\r\n", hopperLevel);
    
    // Error status
    if (errorHandler && errorHandler->hasErrors()) {
        uint8_t errors = errorHandler->getCurrentErrors();
        debugPrintf("Errors (0x%02X):", errors);
        if (errors & ErrorHandler::ERROR_HOPPER_LOW) debugPrint(" HOPPER_LOW");
        if (errors & ErrorHandler::ERROR_MIX_TIME_EXCEEDED) debugPrint(" MIX_TIME_EXCEEDED");
        if (errors & ErrorHandler::ERROR_WATER_SENSOR_FAILURE) debugPrint(" WATER_SENSOR_FAILURE");
        debugPrint("\r\n");
    } else {
        debugPrintln("No active errors");
    }
    
    // System resources
    debugPrintf("Free Heap: %lu bytes\r\n", ESP.getFreeHeap());
    debugPrintf("WiFi RSSI: %d dBm\r\n", WiFi.RSSI());
    debugPrintln("==================\n");
}