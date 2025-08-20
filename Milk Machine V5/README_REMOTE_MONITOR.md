# Remote Serial Monitor for Milk Machine V5

## Overview

This system provides 100% reliable, bulletproof remote monitoring of your ESP32-based Milk Machine serial output with multiple access methods and failover capabilities.

## Features

- **TCP Server (Port 23)**: Telnet-style real-time streaming
- **Web Interface (Port 80)**: Browser-based monitoring with log buffer
- **Dual Logging**: All important messages sent to both Serial and remote clients
- **Auto-Reconnection**: Handles network interruptions gracefully
- **Basic Authentication**: Password protection for web interface
- **Configurable**: Easy to modify ports, buffer sizes, and security settings
- **Multiple Clients**: Support for 2 simultaneous TCP connections
- **Log Buffer**: Keeps recent logs available even after disconnections

## Access Methods

### 1. TCP/Telnet (Real-time)
```bash
# Windows
telnet 192.168.1.16 23

# Linux/Mac
telnet 192.168.1.16 23
# OR
nc 192.168.1.16 23

# PuTTY (Windows)
# Host: 192.168.1.16, Port: 23, Connection type: Raw
```

### 2. Web Interface
Open browser to: `http://192.168.1.16/`
- Username: `admin`
- Password: `Gliders1!`
- Auto-refreshes every 5 seconds
- Shows recent logs with timestamps
- Raw logs available at `/logs` endpoint

### 3. Python Test Script
```bash
python test_monitor.py          # Run tests
python test_monitor.py monitor  # Continuous monitoring
```

## Configuration

Edit [`src/monitor_config.h`](src/monitor_config.h) to customize:

```cpp
#define MONITOR_TCP_PORT 23        // Telnet port
#define MONITOR_WEB_PORT 80        // Web interface port
#define MONITOR_ENABLE_AUTH true   // Enable authentication
#define MONITOR_PASSWORD "Gliders1!"  // Web password
#define MONITOR_MAX_CLIENTS 2      // Max simultaneous TCP clients
#define MONITOR_LOG_BUFFER_SIZE 4096  // Web log buffer size
```

## Integration

The monitor is fully integrated into your existing code:

- **RLOG_PRINTLN(message)**: Logs to both Serial and remote clients
- **RLOG_PRINTF(format, ...)**: Formatted logging to both outputs
- **Wi-Fi Event-Based Lifecycle**: Tied to Wi-Fi connection events for maximum reliability
- Starts automatically on SYSTEM_EVENT_STA_GOT_IP
- Stops cleanly on SYSTEM_EVENT_STA_DISCONNECTED to free resources
- No impact on critical milk machine operations

## Reliability Features

### Network Interruption Handling
- **Wi-Fi Event-Based Recovery**: Monitor restarts automatically when Wi-Fi reconnects
- TCP clients automatically reconnect when network is restored
- Web interface shows current connection status
- Log buffer preserves recent messages during outages
- System continues operating normally without remote connections
- Clean resource management during disconnections

### Multiple Access Points
- If TCP fails, use web interface
- If web fails, use TCP
- Local serial always available as fallback
- Multiple simultaneous connections supported

### Safety
- Monitor never interferes with milk machine operations
- Graceful handling of client disconnections
- Memory management prevents buffer overflows
- Authentication prevents unauthorized access

## Monitoring What Matters

The system automatically logs:
- **System State Changes**: IDLE → MIXING → POST_MIX, etc.
- **Level Switch Events**: Milk level detection
- **Fault Conditions**: Any errors or safety triggers
- **System Status**: Every 10 seconds with WiFi, heap, uptime
- **Boot Information**: Reset reasons and initialization
- **Remote Client Info**: Number of connected monitors

## Troubleshooting

### Can't Connect via TCP
```bash
# Test if port is open
nmap -p 23 192.168.1.16

# Check if machine is online
ping 192.168.1.16
```

### Web Interface Not Loading
1. Check WiFi connection on machine
2. Try: `http://192.168.1.16/` (not https)
3. Use correct credentials: admin/Gliders1!
4. Clear browser cache

### Missing Logs
- Recent logs stored in 4KB buffer
- Older logs naturally roll off
- TCP provides real-time stream
- Check Serial output as ultimate fallback

### Network Issues
- System handles WiFi reconnections automatically
- Check router/network stability
- Verify static IP configuration (192.168.1.16)
- Monitor attempts exponential backoff

## Example Output

```
=== System Status ===
State: IDLE
Level Switch: HIGH (milk sufficient)
WiFi: Connected
Remote Clients: 2
Uptime: 12847 s
Free Heap: 187532 bytes
====================
```

## Security Notes

- Change default password in [`src/monitor_config.h`](src/monitor_config.h)
- Web interface uses HTTP basic auth
- TCP connections are unencrypted (like standard telnet)
- Access limited to local network only
- Consider VPN for remote access over internet

## Quick Start

1. **Upload Code**: Flash the updated firmware to your ESP32
2. **Wait for Boot**: Watch serial output for "Remote monitor started"
3. **Test TCP**: `telnet 192.168.1.16 23`
4. **Test Web**: Open `http://192.168.1.16/` in browser
5. **Monitor**: Use either method for continuous monitoring

## Files Added/Modified

- [`src/remote_monitor.h`](src/remote_monitor.h) - Header file
- [`src/remote_monitor.cpp`](src/remote_monitor.cpp) - Implementation
- [`src/monitor_config.h`](src/monitor_config.h) - Configuration
- [`src/main.cpp`](src/main.cpp) - Integration (minimal changes)
- [`test_monitor.py`](test_monitor.py) - Test script
- [`README_REMOTE_MONITOR.md`](README_REMOTE_MONITOR.md) - This documentation

The system is designed to be **simple, reliable, and bulletproof** - exactly what you need for critical remote monitoring.