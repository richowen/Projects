# CCTV Alert System

ESP32-based CCTV alert system with configurable LED and sound alerts. The system listens for HTTP requests to trigger different alert types based on detected objects.

## Quick Start

Replace `<DEVICE_IP>` with your ESP32's IP address in all URLs below.

### Alert Trigger URLs

Trigger alerts for different detected objects:

```
http://<DEVICE_IP>/trigger?type=car
http://<DEVICE_IP>/trigger?type=truck
http://<DEVICE_IP>/trigger?type=motorcycle
http://<DEVICE_IP>/trigger?type=pedestrian
```

### Fire Alarm Control

```
http://<DEVICE_IP>/fire       # Activate fire alarm
http://<DEVICE_IP>/stop       # Stop fire alarm
```

### Configuration & Testing

```
http://<DEVICE_IP>/config     # Web UI for alert configuration
http://<DEVICE_IP>/health     # System health check
```

Test alert endpoints (including unknown type):
```
http://<DEVICE_IP>/test?type=car
http://<DEVICE_IP>/test?type=truck
http://<DEVICE_IP>/test?type=motorcycle
http://<DEVICE_IP>/test?type=pedestrian
http://<DEVICE_IP>/test?type=unknown
```

## Alert Types

| Type | LED Pattern | Duration | Priority | Sound Pattern |
|------|-------------|----------|----------|---------------|
| **Car** | Blink | 3s | High | Rising tones (1000 → 1500 → 2000 Hz) |
| **Truck** | Breathe | 5s | High | Alternating low tones (400 ⇄ 600 Hz) |
| **Motorcycle** | Strobe | 2s | Medium | High pitched (1500 → 1800 Hz) |
| **Pedestrian** | Solid | 4s | Low | Beeps (1000 Hz) |
| **Unknown** | Blink | 2.5s | Medium | Varied tones (500 → 700 → 900 Hz) |

## Hardware Configuration

- **LED Pin**: Configured in [`config.h`](include/config.h)
- **Speaker Pin**: Configured in [`config.h`](include/config.h)
- **Platform**: ESP32 (PlatformIO project)

## Configuration

Alert configurations can be customized via:
1. **Web UI**: Navigate to `http://<DEVICE_IP>/config`
2. **Code**: Modify defaults in [`src/alerts.cpp`](src/alerts.cpp)

Each alert supports:
- Custom LED patterns (solid, blink, breathe, strobe)
- Configurable duration
- Up to 5 sound tones with individual frequencies and durations
- Priority levels (low, medium, high)

## Project Structure

```
├── src/
│   ├── main.cpp       # Main application entry
│   ├── alerts.cpp     # Alert system implementation
│   ├── network.cpp    # Web server and WiFi management
│   ├── config.cpp     # Configuration management
│   └── system.cpp     # System monitoring
├── include/
│   ├── alerts.h       # Alert type definitions
│   ├── network.h      # Network functions
│   ├── config.h       # Global configuration
│   └── system.h       # System health monitoring
└── platformio.ini     # PlatformIO configuration
```

## API Reference

### Trigger Alert
**GET** `/trigger?type=<alert_type>`

Triggers an alert for the specified type. Valid types: `car`, `truck`, `motorcycle`, `pedestrian`.

**Response**: `{"status":"success","message":"Alert triggered: <type>"}`

### Fire Alarm Control
**GET** `/fire` - Activates fire alarm  
**GET** `/stop` - Deactivates fire alarm

### System Health
**GET** `/health` - Returns system status JSON with memory, uptime, and WiFi information

### Configuration
**GET** `/api/config` - Get current alert configurations as JSON  
**POST** `/api/config` - Update alert configurations

## Building & Uploading

```bash
# Build project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```
