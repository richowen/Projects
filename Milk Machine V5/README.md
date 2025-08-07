# Milk Mixer Control System (V5)

Production-grade automated milk mixing system for ESP32 (FireBeetle). V5 is a full rewrite with modular architecture, non-blocking control loop, and robust telemetry, preserving all V4 behaviors and pin assignments.

## Hardware Configuration

### ESP32 Board
- Board: FireBeetle ESP32
- Static IP: 192.168.1.16
- WiFi: SSID "WiFi", password "Gliders1!"

### Pin Assignments
```cpp
// Active LOW relays, INPUT_PULLUP switches (see src/core/Config.h)
const int RELAY_AUGER = 25;
const int RELAY_AGITATOR = 26;
const int RELAY_MIXER = 16;
const int RELAY_WATER = 17;
const int LEVEL_SWITCH = 12;     // input pull-up, LOW=active
#define WASH_STANDBY_PIN 23      // input pull-up, LOW=active
#define WASH_DISPENSE_PIN 5      // input pull-up, LOW=active
```

### LCD
- DFRobot RGB LCD 1602, I2C addr 0x2D, 16x2
- Color scheme: Blue (idle), Green (mix/post), Red (error), Cyan (wash), Yellow (OTA)

## System Operation

### Automatic Mixing Cycle
1) Trigger: Level switch active (LOW) -> start full cycle (auger+agitator+mixer+water).
2) When level restores (HIGH), transition to Post-Mix.
3) Post-Mix: mixer-only for 5 seconds.
4) Return to Idle.

### Periodic Mixing (Idle)
- Every 5 minutes, mixer-only for 5 seconds to keep milk stirred.

### Wash Mode
- Hold WASH_STANDBY_PIN LOW to enter wash.
- WASH_DISPENSE_PIN LOW enables water; 30-second safety timeout.
- Release standby to exit wash.

### Error Handling
- Timeout error if mixing exceeds 60s.
- Level switch error if stuck 60s during mixing.
- Auto-recovery after 5 minutes back to idle.

## MQTT Integration

### Broker
```cpp
const char* mqtt_server = "192.168.1.3";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "richowen";
const char* mqtt_pass   = "p";
```

### Topics
```
milk_mixer/status             // idle|mixing|post_mixing|wash_mode|error_*
milk_mixer/error              // error messages
milk_mixer/available          // online|offline (LWT)
milk_mixer/data/total_mixes   // uint
milk_mixer/data/session_mixes // uint
milk_mixer/data/uptime_hours  // uint
milk_mixer/data/error_count   // uint
milk_mixer/data/last_mix      // millis timestamp
```

- Initial payloads published on connect, periodic data every 60s, plus event-driven updates.

## OTA Updates
- Guarded OTA (ArduinoOTA) operates only in safe states (IDLE/WASH).
- OTA progress shown on LCD; disabled during mixing/post-mixing.

## Architecture Overview
- Core
  - src/core/Config.h: constants: pins, timings, WiFi/MQTT, LCD colors.
  - src/core/Timers.h: elapsed(), Debounce for stable inputs.
  - src/core/DataStore.h: Preferences persistence (totalMixes, errorCount, lastMixTime, sessionMixes, uptime).
- HAL
  - src/hal/Pins.h: pin init and active-low helpers.
  - src/hal/Relays.h: auger/agitator/mixer/water control with allOn/allOff.
  - src/hal/Inputs.h: debounced level/wash inputs.
  - src/hal/Lcd.h: LCD wrapper, diff-only updates to reduce flicker and I2C traffic.
- Networking
  - src/net/WiFiManager.h: static IP connect with retries and LCD feedback.
  - src/net/MqttClient.h: publish-only MQTT with LWT + 60s reporting.
  - src/net/OtaManager.h: guarded OTA lifecycle and LCD progress.
- App
  - src/app/App.h/.cpp: state machine (IDLE, MIXING, POST_MIXING, ERROR, WASH), periodic mixing, safety, telemetry integration.
  - src/main.cpp: delegates to App.

## Build and Flash (PlatformIO)
platformio.ini is preconfigured:
```ini
[env:firebeetle32]
platform = espressif32
board = firebeetle32
framework = arduino
monitor_speed = 115200
lib_deps =
    knolleary/PubSubClient
    dfrobot/DFRobot_RGBLCD1602
build_flags =
    -D MQTT_MAX_PACKET_SIZE=512
```

Steps:
1) USB build/flash: pio run -e firebeetle32 -t upload
2) Serial monitor: pio device monitor -b 115200
3) OTA (optional, device must be idle/wash):
   - Add to platformio.ini:
     ```
     upload_protocol = espota
     upload_port = 192.168.1.16
     upload_flags =
         --host_port=3232
     ```
   - Then: pio run -e firebeetle32 -t upload

## Home Assistant Snippet
(unchanged from V4; see sensors and binary_sensors using the topics listed above.)

## Troubleshooting
- LCD not found: verify I2C wiring and address 0x2D.
- No MQTT: verify broker IP/creds and that WiFi connected; check LWT topic milk_mixer/available.
- OTA blocked: ensure system is idle or in wash; mixing/post-mix disables OTA handling.
- Timing: adjust constants under src/core/Config.h::Times.

## Notes
- Core operation continues without WiFi/MQTT.
- All timings and topics are centralized for easy tuning.
- Preferences namespace: "milkmixer".
