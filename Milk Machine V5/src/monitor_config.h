#ifndef MONITOR_CONFIG_H
#define MONITOR_CONFIG_H

#include "secrets.h"  // [Fix #4] credentials from gitignored file

// ================== Remote Monitor Configuration =================

// Network ports
#define MONITOR_TCP_PORT  23   // Telnet-style log stream
#define MONITOR_WEB_PORT  80   // HTTP log viewer

// Security — [Fix #5] use integer 1/0, not bool literal (fragile in #if context)
#define MONITOR_ENABLE_AUTH    1                   // 1 = enabled, 0 = disabled
#define MONITOR_PASSWORD       SECRET_MONITOR_PASS // HTTP Basic auth password (user = "admin")
#define MONITOR_TCP_PASSWORD   SECRET_MONITOR_PASS // TCP challenge password
#define TCP_AUTH_TIMEOUT_MS    5000UL              // ms to respond before disconnect

// Buffer settings
#define MONITOR_LOG_BUFFER_SIZE  4096  // Bytes in the in-RAM circular log buffer
#define MONITOR_MAX_CLIENTS      2     // Maximum simultaneous TCP clients

// [Fix #10] Force-close silent / ghost TCP connections after this idle period
#define CLIENT_IDLE_TIMEOUT_MS  (5UL * 60UL * 1000UL)  // 5 minutes

// TODO: Rate limiting — defined here for future implementation, not yet wired up
// #define MONITOR_MAX_CONNECTIONS_PER_MINUTE  10
// #define MONITOR_ENABLE_RATE_LIMITING        1

// TODO: Auto-restart on monitor hang — not yet implemented
// #define MONITOR_AUTO_RESTART_ON_HANG        1
// #define MONITOR_RESTART_DELAY_MS            5000

#endif
