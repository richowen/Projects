#ifndef MONITOR_CONFIG_H
#define MONITOR_CONFIG_H

// ================== Remote Monitor Configuration =================

// Network ports
#define MONITOR_TCP_PORT 23        // Telnet port
#define MONITOR_WEB_PORT 80        // Web interface port

// Security settings
#define MONITOR_ENABLE_AUTH true   // Enable simple authentication
#define MONITOR_PASSWORD "Gliders1!"  // Simple password protection

// Buffer settings
#define MONITOR_LOG_BUFFER_SIZE 4096  // Bytes to keep in web log buffer
#define MONITOR_MAX_CLIENTS 2         // Maximum simultaneous TCP clients

// Reliability settings  
#define MONITOR_AUTO_RESTART_ON_HANG true  // Auto-restart if monitor hangs
#define MONITOR_RESTART_DELAY_MS 5000      // Delay before restart attempt

// Rate limiting (prevent spam/DoS)
#define MONITOR_MAX_CONNECTIONS_PER_MINUTE 10
#define MONITOR_ENABLE_RATE_LIMITING true

#endif