#include "remote_monitor.h"
#include <string.h>
#include <stdarg.h>

RemoteMonitor remoteMonitor;

// =====================================================================
// Constructor
// =====================================================================
RemoteMonitor::RemoteMonitor()
    : tcpServer(nullptr), webServer(nullptr), enabled(false),
      logHead(0), logLen(0)
{
    memset(logBuf, 0, sizeof(logBuf));
    memset(lastClientActivity, 0, sizeof(lastClientActivity));
#if MONITOR_ENABLE_AUTH
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        clientAuthState[i]     = AUTH_WAITING;
        clientAuthTimestamp[i] = 0;
        clientAuthLen[i]       = 0;
        memset(clientAuthBuf[i], 0, sizeof(clientAuthBuf[i]));
    }
#endif
}

// =====================================================================
// begin() — call from Core 1 (handleWiFiOTA) only [Fix #1]
// =====================================================================
bool RemoteMonitor::begin(uint16_t tcpPort, uint16_t webPort) {
    if (WiFi.status() != WL_CONNECTED) return false;

    // Clean up any previous server instances
    if (tcpServer) { delete tcpServer; tcpServer = nullptr; }
    if (webServer)  { delete webServer;  webServer  = nullptr; }

    tcpServer = new WiFiServer(tcpPort);
    tcpServer->begin();

    webServer = new WebServer(webPort);
    webServer->on("/",     [this]() { this->handleRoot(); });
    webServer->on("/logs", [this]() { this->handleLogs(); });
    webServer->begin();

    // Initialise per-client state
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        tcpClients[i]         = WiFiClient();
        lastClientActivity[i] = 0;
#if MONITOR_ENABLE_AUTH
        clientAuthState[i]     = AUTH_WAITING;
        clientAuthTimestamp[i] = 0;
        clientAuthLen[i]       = 0;
        memset(clientAuthBuf[i], 0, sizeof(clientAuthBuf[i]));
#endif
    }

    enabled = true;
    Serial.printf("Remote monitor started: TCP port %u, Web port %u\n", tcpPort, webPort);
    return true;
}

// =====================================================================
// handle() — called every loop() iteration from Core 1 only [Fix #1]
// =====================================================================
void RemoteMonitor::handle() {
    if (!enabled) return;

    unsigned long now = millis();

    // --- Accept new TCP clients ---
    if (tcpServer && tcpServer->hasClient()) {
        WiFiClient newClient = tcpServer->available();
        bool added = false;
        for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
            if (!tcpClients[i] || !tcpClients[i].connected()) {
                if (tcpClients[i]) tcpClients[i].stop();
                tcpClients[i]         = newClient;
                // [Round2-B] Non-blocking writes: avoid loop stall on slow/stuck peer
                tcpClients[i].setNoDelay(true);
                tcpClients[i].setTimeout(50);
                lastClientActivity[i] = now;
#if MONITOR_ENABLE_AUTH
                // Send password challenge and start auth timeout [Fix #2]
                clientAuthState[i]     = AUTH_WAITING;
                clientAuthTimestamp[i] = now;
                clientAuthLen[i]       = 0;
                memset(clientAuthBuf[i], 0, sizeof(clientAuthBuf[i]));
                tcpClients[i].print("Password: ");
#else
                tcpClients[i].println("=== Milk Machine Remote Monitor ===");
#endif
                added = true;
                break;
            }
        }
        if (!added) {
            newClient.println("Server full.");
            newClient.stop();
        }
    }

    // --- Service existing clients ---
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].connected()) {
            if (tcpClients[i]) tcpClients[i].stop();
            continue;
        }

#if MONITOR_ENABLE_AUTH
        if (clientAuthState[i] == AUTH_WAITING) {
            // Enforce auth timeout [Fix #2]
            if ((now - clientAuthTimestamp[i]) > TCP_AUTH_TIMEOUT_MS) {
                tcpClients[i].println("\r\nTimeout.");
                tcpClients[i].stop();
                continue;
            }
            // Collect incoming bytes looking for newline
            while (tcpClients[i].available()) {
                char c = (char)tcpClients[i].read();
                if (c == '\n') {
                    // Strip trailing \r
                    while (clientAuthLen[i] > 0 &&
                           clientAuthBuf[i][clientAuthLen[i] - 1] == '\r') {
                        clientAuthLen[i]--;
                    }
                    clientAuthBuf[i][clientAuthLen[i]] = '\0';

                    if (strcmp(clientAuthBuf[i], MONITOR_TCP_PASSWORD) == 0) {
                        clientAuthState[i]    = AUTH_OK;
                        lastClientActivity[i] = now;
                        tcpClients[i].println("=== Milk Machine Remote Monitor ===");
                    } else {
                        tcpClients[i].println("Access denied.");
                        tcpClients[i].stop();
                    }
                    clientAuthLen[i] = 0;
                    break;
                } else if (clientAuthLen[i] < (uint8_t)(sizeof(clientAuthBuf[i]) - 1)) {
                    clientAuthBuf[i][clientAuthLen[i]++] = c;
                }
            }
            continue;  // don't apply idle timeout to pending-auth clients
        }
#endif

        // [Fix #10] Force-close ghost / idle connections
        if ((now - lastClientActivity[i]) > CLIENT_IDLE_TIMEOUT_MS) {
            tcpClients[i].stop();
        }
    }

    // --- Web server ---
    if (webServer) webServer->handleClient();
}

// =====================================================================
// Circular buffer — fixed-size, no heap fragmentation [Fix #3]
// =====================================================================
// [Round2-D] memcpy-based writer: up to 2 contiguous chunks, no per-byte modulo.
void RemoteMonitor::appendLog(const char* s, size_t len) {
    if (len == 0) return;
    if (len >= LOG_BUF_SIZE) {
        // Source larger than buffer — keep only the final LOG_BUF_SIZE bytes
        s   += (len - LOG_BUF_SIZE);
        len  = LOG_BUF_SIZE;
        memcpy(logBuf, s, LOG_BUF_SIZE);
        logHead = 0;
        logLen  = LOG_BUF_SIZE;
        return;
    }
    size_t writePos = (logHead + logLen) % LOG_BUF_SIZE;
    size_t tail     = LOG_BUF_SIZE - writePos;
    size_t first    = len < tail ? len : tail;
    memcpy(logBuf + writePos, s, first);
    if (len > first) memcpy(logBuf, s + first, len - first);

    if (logLen + len <= LOG_BUF_SIZE) {
        logLen += len;
    } else {
        size_t overflow = (logLen + len) - LOG_BUF_SIZE;
        logHead = (logHead + overflow) % LOG_BUF_SIZE;
        logLen  = LOG_BUF_SIZE;
    }
}

// Linearise the circular buffer into a transient String for HTTP responses.
// Allocating once per HTTP request is acceptable; the ongoing churn is eliminated.
String RemoteMonitor::getLogString() const {
    if (logLen == 0) return String();
    String s;
    if (!s.reserve((unsigned int)logLen)) {
        return String("(log unavailable: low memory)");
    }
    // The buffer may wrap: chunk1 = contiguous bytes from logHead to end of array
    size_t chunk1 = LOG_BUF_SIZE - logHead;
    if (chunk1 > logLen) chunk1 = logLen;
    s.concat(logBuf + logHead, (unsigned int)chunk1);
    if (chunk1 < logLen) {
        s.concat(logBuf, (unsigned int)(logLen - chunk1));
    }
    return s;
}

// =====================================================================
// println / printf
// =====================================================================
void RemoteMonitor::println(const String& msg) { println(msg.c_str()); }

// [Round2-A/E] Centralised writer: always terminates with exactly one '\n'
// in both the log buffer and the TCP stream.
void RemoteMonitor::writeRecord(const char* msg, size_t msgLen) {
    char ts[20];
    int tsLen = snprintf(ts, sizeof(ts), "%lu: ", millis());
    if (tsLen < 0) tsLen = 0;

    // Trim any trailing newline(s) from caller so we control termination
    while (msgLen > 0 && (msg[msgLen - 1] == '\n' || msg[msgLen - 1] == '\r')) {
        msgLen--;
    }

    appendLog(ts, (size_t)tsLen);
    appendLog(msg, msgLen);
    appendLog("\n", 1);

    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (!tcpClients[i] || !tcpClients[i].connected()) continue;
#if MONITOR_ENABLE_AUTH
        if (clientAuthState[i] != AUTH_OK) continue;
#endif
        // [Round2-B] Skip write if peer TX buffer is full — avoid stall
        if (tcpClients[i].availableForWrite() < (int)(tsLen + msgLen + 2)) continue;
        tcpClients[i].write((const uint8_t*)ts,  tsLen);
        tcpClients[i].write((const uint8_t*)msg, msgLen);
        tcpClients[i].write((const uint8_t*)"\r\n", 2);
        lastClientActivity[i] = millis();
    }
}

void RemoteMonitor::println(const char* msg) {
    if (!enabled || !msg) return;
    writeRecord(msg, strlen(msg));
}

void RemoteMonitor::printf(const char* format, ...) {
    if (!enabled) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer) - 10, format, args);
    va_end(args);

    size_t len;
    if (n < 0) {
        return;
    } else if (n >= (int)(sizeof(buffer) - 10)) {
        memcpy(buffer + sizeof(buffer) - 11, "...[trunc]", 10);
        buffer[sizeof(buffer) - 1] = '\0';
        len = sizeof(buffer) - 1;
    } else {
        len = (size_t)n;
    }
    writeRecord(buffer, len);
}

// =====================================================================
// getClientCount
// =====================================================================
uint8_t RemoteMonitor::getClientCount() const {
    uint8_t count = 0;
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        // WiFiClient::connected() is non-const in the core — cast away const;
        // this method only reads connection state.
        if (const_cast<WiFiClient&>(tcpClients[i]).connected()) count++;
    }
    return count;
}

// =====================================================================
// HTTP handlers
// =====================================================================
// [Round2-C] Chunked output — no 4 KB+ transient String allocations.
static const char HTML_HEAD[] PROGMEM =
    "<!DOCTYPE html><html><head><title>Milk Machine Monitor</title>"
    "<meta charset='utf-8'><meta http-equiv='refresh' content='5'><style>"
    "body{font-family:monospace;background:#000;color:#0f0;padding:20px;}"
    "h1{color:#fff;text-align:center;}"
    ".log{background:#111;border:1px solid #333;padding:10px;"
    "height:400px;overflow-y:scroll;white-space:pre-wrap;}"
    "</style></head><body><h1>Milk Machine V5 Monitor</h1><p>"
    "Status: <span style='color:#0f0'>ONLINE</span> | TCP Clients: ";
static const char HTML_MID[]  PROGMEM = " | Uptime: ";
static const char HTML_MID2[] PROGMEM = "s</p><div class='log'>";
static const char HTML_TAIL[] PROGMEM =
    "</div><p><a href='/logs'>Raw Logs</a> | <a href='/'>Refresh</a></p>"
    "</body></html>";

// Stream circular log buffer to HTTP client, HTML-escaping '<','>','&'.
// Flushes in ~256-byte chunks to keep transient heap flat.
void RemoteMonitor::streamLogEscaped() {
    if (logLen == 0) return;
    char chunk[256];
    size_t pos = 0;
    for (size_t i = 0; i < logLen; i++) {
        char c = logBuf[(logHead + i) % LOG_BUF_SIZE];
        const char* esc = nullptr;
        size_t elen = 0;
        if      (c == '&') { esc = "&amp;";  elen = 5; }
        else if (c == '<') { esc = "&lt;";   elen = 4; }
        else if (c == '>') { esc = "&gt;";   elen = 4; }
        if (esc) {
            if (pos + elen > sizeof(chunk)) {
                webServer->sendContent(chunk, pos); pos = 0;
            }
            memcpy(chunk + pos, esc, elen); pos += elen;
        } else {
            if (pos >= sizeof(chunk)) {
                webServer->sendContent(chunk, pos); pos = 0;
            }
            chunk[pos++] = c;
        }
    }
    if (pos) webServer->sendContent(chunk, pos);
}

void RemoteMonitor::handleRoot() {
#if MONITOR_ENABLE_AUTH
    if (!webServer->authenticate("admin", MONITOR_PASSWORD)) {
        return webServer->requestAuthentication();
    }
#endif
    webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    webServer->send(200, "text/html", "");
    webServer->sendContent_P(HTML_HEAD);
    webServer->sendContent(String(getClientCount()));
    webServer->sendContent_P(HTML_MID);
    webServer->sendContent(String(millis() / 1000UL));
    webServer->sendContent_P(HTML_MID2);
    streamLogEscaped();
    webServer->sendContent_P(HTML_TAIL);
    webServer->sendContent("");  // terminate chunked response
}

void RemoteMonitor::handleLogs() {
#if MONITOR_ENABLE_AUTH
    if (!webServer->authenticate("admin", MONITOR_PASSWORD)) {
        return webServer->requestAuthentication();
    }
#endif
    // Zero-copy: send the two contiguous slices of the circular buffer directly.
    webServer->setContentLength(logLen);
    webServer->send(200, "text/plain", "");
    if (logLen == 0) { webServer->sendContent(""); return; }
    size_t chunk1 = LOG_BUF_SIZE - logHead;
    if (chunk1 > logLen) chunk1 = logLen;
    webServer->sendContent(logBuf + logHead, chunk1);
    if (chunk1 < logLen) {
        webServer->sendContent(logBuf, logLen - chunk1);
    }
    webServer->sendContent("");
}

// =====================================================================
// end() — call from Core 1 only [Fix #1]
// =====================================================================
void RemoteMonitor::end() {
    if (!enabled) return;

    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i]) tcpClients[i].stop();
    }

    if (tcpServer) { tcpServer->end(); delete tcpServer; tcpServer = nullptr; }
    if (webServer)  { webServer->stop();  delete webServer;  webServer  = nullptr; }

    enabled = false;

    // Note [Fix #17]: logBuf is intentionally preserved through disconnect/reconnect
    // cycles to maintain log continuity across WiFi drops. To clear history on each
    // reconnect uncomment the three lines below:
    // logHead = 0;
    // logLen  = 0;
    // memset(logBuf, 0, sizeof(logBuf));

    Serial.println("Remote monitor stopped.");
}
