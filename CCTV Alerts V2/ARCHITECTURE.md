# ESP32 CCTV Alert System - Architecture Plan

## Overview
ESP32-based alert system that receives HTTP requests from a CCTV system and triggers visual (LED strip) and audio (speaker) alerts with different patterns based on alert type.

## Hardware Configuration

### Components
- **Board**: ESP32 DevKit V1
- **LED Strip**: Controlled via MOSFET on GPIO 14
- **Speaker**: Simple tone speaker on GPIO 13 (PWM-driven)

### Pin Assignments
```
GPIO 13 → Speaker (PWM)
GPIO 14 → MOSFET Gate (LED Strip Control)
```

### Power Considerations
- Low power board - keep processing minimal
- Use efficient PWM for tone generation
- LED control via MOSFET switching

## Network Configuration

### WiFi Settings
- **SSID**: IoT
- **Password**: Gliders1!
- **Static IP**: 192.168.1.21
- **Gateway**: 192.168.1.1 (assumed)
- **Subnet**: 255.255.255.0 (assumed)

### HTTP Endpoints
1. `GET/POST 192.168.1.21/trigger/cctv` - CCTV motion/event alert
2. `GET/POST 192.168.1.21/trigger/fire` - Fire alarm alert

## Alert Behaviors

### CCTV Alert (`/trigger/cctv`)
**Duration**: ~3 seconds total
**Pattern**:
- Play 3 ascending tones (e.g., 800Hz → 1000Hz → 1200Hz)
- Flash LED strip 3 times (500ms ON, 500ms OFF)
- Each tone synchronized with LED flash
- Returns to idle state after completion

**Sequence**:
```
Tone 1 (800Hz) + LED ON (500ms) → LED OFF (500ms)
Tone 2 (1000Hz) + LED ON (500ms) → LED OFF (500ms)
Tone 3 (1200Hz) + LED ON (500ms) → LED OFF (500ms)
```

### Fire Alert (`/trigger/fire`)
**Duration**: Continuous until device reset
**Pattern**:
- Continuous alarm tone (alternating frequencies for urgency)
- Rapid LED flashing (e.g., 200ms ON/OFF cycle)
- Cannot be stopped via software - requires physical reset

**Sequence**:
```
Loop forever:
  Alarm tone (e.g., 1500Hz for 200ms, 1000Hz for 200ms)
  LED flash (200ms ON, 200ms OFF)
```

## Software Architecture

### Main Components

#### 1. WiFi Manager
- Connect to WiFi network
- Configure static IP address
- Monitor connection status
- Reconnect on disconnect

#### 2. Web Server
- AsyncWebServer on port 80
- Lightweight request handlers
- Respond quickly to minimize CCTV timeout

#### 3. PWM Tone Generator
- Use ESP32 LEDC peripheral
- Single PWM channel for speaker
- Functions for playing tones at specific frequencies
- Non-blocking tone playback where possible

#### 4. LED Controller
- Simple digitalWrite for MOSFET control
- Timing-based flashing patterns
- Support for both finite and continuous patterns

#### 5. Alert State Machine
```mermaid
stateDiagram-day
    [*] --> Idle
    Idle --> CCTVAlert : /trigger/cctv
    Idle --> FireAlert : /trigger/fire
    CCTVAlert --> PlayTone1 : Start
    PlayTone1 --> PlayTone2 : After 1s
    PlayTone2 --> PlayTone3 : After 1s
    PlayTone3 --> Idle : After 1s
    FireAlert --> AlarmLoop : Start
    AlarmLoop --> AlarmLoop : infinite loop
    AlarmLoop --> [*] : Device Reset Only
```

### Code Structure

#### Main Functions
- `setup()`: Initialize WiFi, server, pins, PWM
- `loop()`: Handle alert state machine, WiFi monitoring
- `handleCCTVAlert()`: Execute CCTV alert sequence
- `handleFireAlert()`: Execute fire alert sequence
- `playTone(frequency, duration)`: Play specific tone via PWM
- `flashLED(duration)`: Flash LED for specified time

#### PWM Configuration
```cpp
// PWM Settings for Speaker
const int SPEAKER_PIN = 13;
const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8;  // 8-bit resolution

// Frequencies
const int CCTV_TONE_1 = 800;   // Hz
const int CCTV_TONE_2 = 1000;
const int CCTV_TONE_3 = 1200;
const int FIRE_TONE_HIGH = 1500;
const int FIRE_TONE_LOW = 1000;
```

## Implementation Priorities

### Critical Features
1. ✓ WiFi connectivity with static IP
2. ✓ HTTP server with both endpoints
3. ✓ Basic tone generation via PWM
4. ✓ LED control via MOSFET

### Core Alert Logic
5. ✓ CCTV alert sequence (3 tones + 3 flashes)
6. ✓ Fire alert continuous loop
7. ✓ Non-blocking execution where safe

### Nice to Have
8. Status LED or serial feedback for debugging
9. WiFi reconnection logic
10. HTTP response messages

## Testing Plan

### Unit Tests
1. WiFi connection and static IP assignment
2. PWM tone generation at various frequencies
3. LED control via MOSFET
4. Web server endpoint responses

### Integration Tests
1. CCTV alert from web browser/curl
2. Fire alert from web browser/curl
3. Multiple rapid CCTV alerts
4. Fire alert persistence across reboot

### Test URLs
```bash
# CCTV Alert
curl http://192.168.1.21/trigger/cctv

# Fire Alert
curl http://192.168.1.21/trigger/fire
```

## Safety Considerations

1. **Fire Alert Priority**: Once triggered, fire alert cannot be cancelled programmatically
2. **MOSFET Protection**: Ensure proper gate resistor if needed (hardware dependent)
3. **Speaker Protection**: Limit PWM duty cycle to safe levels for speaker
4. **Power Management**: Monitor current draw with LED strip active

## Future Enhancements

- Add GET endpoint to query current alert status
- Log alerts to SPIFFS/LittleFS
- Add authentication to prevent unauthorized triggers
- Support for additional alert types
- Adjustable volume (PWM duty cycle) for speaker
- OTA updates for firmware

## Notes

- Keep code simple due to low-power constraint
- Minimize memory usage
- Use ESP32's dual-core capability if needed for simultaneous operations
- Consider watchdog timer for reliability
