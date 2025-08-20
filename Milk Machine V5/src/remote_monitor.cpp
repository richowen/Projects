#include "remote_monitor.h"
#include "monitor_config.h"

RemoteMonitor remoteMonitor;

bool RemoteMonitor::begin(uint16_t tcpPort, uint16_t webPort) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Clean up existing servers
    if (tcpServer) {
        delete tcpServer;
    }
    if (webServer) {
        delete webServer;
    }
    
    // Start TCP server
    tcpServer = new WiFiServer(tcpPort);
    tcpServer->begin();
    
    // Start Web server
    webServer = new WebServer(webPort);
    webServer->on("/", [this]() { this->handleRoot(); });
    webServer->on("/logs", [this]() { this->handleLogs(); });
    webServer->begin();
    
    enabled = true;
    
    // Initialize client array
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        tcpClients[i] = WiFiClient();
    }
    
    // Initialize log buffer
    logBuffer.reserve(MONITOR_LOG_BUFFER_SIZE);
    
    Serial.printf("Remote monitor started: TCP port %d, Web port %d\n", tcpPort, webPort);
    return true;
}

void RemoteMonitor::handle() {
    if (!enabled) return;
    
    // Handle TCP server
    if (tcpServer && tcpServer->hasClient()) {
        WiFiClient newClient = tcpServer->available();
        
        // Find empty slot or replace oldest
        bool added = false;
        for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
            if (!tcpClients[i] || !tcpClients[i].connected()) {
                if (tcpClients[i]) tcpClients[i].stop();
                tcpClients[i] = newClient;
                tcpClients[i].println("=== Milk Machine Remote Monitor ===");
                added = true;
                break;
            }
        }
        
        // No space - reject
        if (!added) {
            newClient.println("Server full");
            newClient.stop();
        }
    }
    
    // Clean up disconnected TCP clients
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i] && !tcpClients[i].connected()) {
            tcpClients[i].stop();
        }
    }
    
    // Handle web server
    if (webServer) {
        webServer->handleClient();
    }
}

void RemoteMonitor::println(const String& msg) {
    println(msg.c_str());
}

void RemoteMonitor::println(const char* msg) {
    if (!enabled) return;
    
    // Add to log buffer with timestamp
    String timestampedMsg = String(millis()) + ": " + String(msg);
    logBuffer += timestampedMsg + "\n";
    
    // Keep buffer manageable
    while (logBuffer.length() > (MONITOR_LOG_BUFFER_SIZE - 512)) {
        int firstNewline = logBuffer.indexOf('\n');
        if (firstNewline > 0) {
            logBuffer = logBuffer.substring(firstNewline + 1);
        } else {
            break;
        }
    }
    
    // Send to TCP clients
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i] && tcpClients[i].connected()) {
            tcpClients[i].println(msg);
        }
    }
}

void RemoteMonitor::printf(const char* format, ...) {
    if (!enabled) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Add to log buffer with timestamp
    String timestampedMsg = String(millis()) + ": " + String(buffer);
    logBuffer += timestampedMsg;
    
    // Keep buffer manageable
    while (logBuffer.length() > (MONITOR_LOG_BUFFER_SIZE - 512)) {
        int firstNewline = logBuffer.indexOf('\n');
        if (firstNewline > 0) {
            logBuffer = logBuffer.substring(firstNewline + 1);
        } else {
            break;
        }
    }
    
    // Send to TCP clients
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i] && tcpClients[i].connected()) {
            tcpClients[i].print(buffer);
        }
    }
}

uint8_t RemoteMonitor::getClientCount() {
    uint8_t count = 0;
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i].connected()) {
            count++;
        }
    }
    return count;
}

void RemoteMonitor::handleRoot() {
    // Basic authentication if enabled
    #if MONITOR_ENABLE_AUTH
    if (!webServer->authenticate("admin", MONITOR_PASSWORD)) {
        return webServer->requestAuthentication();
    }
    #endif
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Milk Machine Monitor</title>";
    html += "<meta charset='utf-8'>";
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<style>body{font-family:monospace;background:#000;color:#0f0;padding:20px;}";
    html += "h1{color:#fff;text-align:center;}";
    html += ".log{background:#111;border:1px solid #333;padding:10px;height:400px;overflow-y:scroll;white-space:pre-wrap;}";
    html += "</style></head><body>";
    html += "<h1>🥛 Milk Machine V5 Monitor</h1>";
    html += "<p>Status: <span style='color:#0f0'>ONLINE</span> | ";
    html += "TCP Clients: " + String(getClientCount()) + " | ";
    html += "Uptime: " + String(millis()/1000) + "s</p>";
    html += "<div class='log'>" + logBuffer + "</div>";
    html += "<p><a href='/logs'>Raw Logs</a> | <a href='/'>Refresh</a></p>";
    html += "</body></html>";
    
    webServer->send(200, "text/html", html);
}

void RemoteMonitor::handleLogs() {
    // Basic authentication if enabled
    #if MONITOR_ENABLE_AUTH
    if (!webServer->authenticate("admin", MONITOR_PASSWORD)) {
        return webServer->requestAuthentication();
    }
    #endif
    
    webServer->send(200, "text/plain", logBuffer);
}

void RemoteMonitor::end() {
    if (!enabled) return;
    
    for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
        if (tcpClients[i]) {
            tcpClients[i].stop();
        }
    }
    
    if (tcpServer) {
        tcpServer->end();
        delete tcpServer;
        tcpServer = nullptr;
    }
    
    if (webServer) {
        webServer->stop();
        delete webServer;
        webServer = nullptr;
    }
    
    enabled = false;
    Serial.println("Remote monitor stopped");
}