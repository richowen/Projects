#ifndef REMOTE_MONITOR_H
#define REMOTE_MONITOR_H

#include <WiFi.h>
#include <WebServer.h>
#include "monitor_config.h"

// Simple TCP server + Web interface for streaming serial output
class RemoteMonitor {
private:
    WiFiServer* tcpServer;
    WebServer* webServer;
    WiFiClient tcpClients[MONITOR_MAX_CLIENTS];  // Configurable simultaneous connections
    String logBuffer;          // Recent logs for web interface
    bool enabled;
    
    void handleRoot();
    void handleLogs();
    
public:
    RemoteMonitor() : tcpServer(nullptr), webServer(nullptr), enabled(false) {}
    
    bool begin(uint16_t tcpPort = 23, uint16_t webPort = 80);
    void handle();
    void println(const String& msg);
    void println(const char* msg);
    void printf(const char* format, ...);
    void end();
    
    bool isEnabled() const { return enabled; }
    uint8_t getClientCount();
};

extern RemoteMonitor remoteMonitor;

// Convenience macros for dual logging
#define RLOG_PRINTLN(msg) do { Serial.println(msg); if(remoteMonitor.isEnabled()) remoteMonitor.println(msg); } while(0)
#define RLOG_PRINTF(fmt, ...) do { Serial.printf(fmt, ##__VA_ARGS__); if(remoteMonitor.isEnabled()) remoteMonitor.printf(fmt, ##__VA_ARGS__); } while(0)

#endif