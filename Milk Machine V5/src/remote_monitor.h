#ifndef REMOTE_MONITOR_H
#define REMOTE_MONITOR_H

#include <WiFi.h>
#include <WebServer.h>
#include "monitor_config.h"

// TCP + HTTP log server for the Milk Machine.
// All public methods are called from Core 1 (loop task) only.
// Do NOT call begin() or end() from a WiFi event handler — see main.cpp
// handleWiFiOTA() for the correct lifecycle pattern. [Fix #1]
class RemoteMonitor {
public:
    RemoteMonitor();

    bool    begin(uint16_t tcpPort = MONITOR_TCP_PORT, uint16_t webPort = MONITOR_WEB_PORT);
    void    handle();
    void    println(const String& msg);
    void    println(const char* msg);
    void    printf(const char* format, ...);
    void    end();

    bool    isEnabled()     const { return enabled; }
    uint8_t getClientCount() const;

private:
    // ---- Servers ----
    WiFiServer* tcpServer;
    WebServer*  webServer;
    WiFiClient  tcpClients[MONITOR_MAX_CLIENTS];
    bool        enabled;

    // ---- Circular log buffer [Fix #3] ----
    // Fixed-size char array — no heap fragmentation from String append/trim.
    static constexpr size_t LOG_BUF_SIZE = MONITOR_LOG_BUFFER_SIZE;
    char   logBuf[LOG_BUF_SIZE];
    size_t logHead;  // index of the oldest stored byte
    size_t logLen;   // number of bytes currently stored

    void   appendLog(const char* s, size_t len);
    String getLogString() const;  // linearise buffer into a transient String for HTTP

    // ---- Per-client state ----
    unsigned long lastClientActivity[MONITOR_MAX_CLIENTS];  // for idle timeout [Fix #10]

#if MONITOR_ENABLE_AUTH
    // Per-client TCP auth handshake state [Fix #2]
    enum ClientAuthState : uint8_t { AUTH_WAITING, AUTH_OK };
    ClientAuthState clientAuthState[MONITOR_MAX_CLIENTS];
    unsigned long   clientAuthTimestamp[MONITOR_MAX_CLIENTS];
    char            clientAuthBuf[MONITOR_MAX_CLIENTS][64];
    uint8_t         clientAuthLen[MONITOR_MAX_CLIENTS];
#endif

    // ---- Private helpers ----
    void writeRecord(const char* msg, size_t msgLen);   // [Round2-A/E]
    void streamLogEscaped();                            // [Round2-C]
    void handleRoot();
    void handleLogs();
};

extern RemoteMonitor remoteMonitor;

// Dual-log macros: always write to Serial; write to monitor only when enabled.
#define RLOG_PRINTLN(msg) \
    do { Serial.println(msg); \
         if (remoteMonitor.isEnabled()) remoteMonitor.println(msg); } while (0)

#define RLOG_PRINTF(fmt, ...) \
    do { Serial.printf(fmt, ##__VA_ARGS__); \
         if (remoteMonitor.isEnabled()) remoteMonitor.printf(fmt, ##__VA_ARGS__); } while (0)

#endif
