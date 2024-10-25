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
                telnetClients[i].println("\nCurrent System Status:");
                telnetClients[i].printf("Free Heap: %lu bytes\r\n", ESP.getFreeHeap());
                telnetClients[i].printf("WiFi RSSI: %d dBm\r\n", WiFi.RSSI());

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
                telnetClients[i].write(c);  // Echo back to client
            }
        } else if (telnetClients[i]) {
            // Clean up if the client has disconnected
            telnetClients[i].stop();
        }
    }
}
