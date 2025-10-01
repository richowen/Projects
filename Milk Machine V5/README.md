### Pin Assignments
```
// Active LOW relays
const int RELAY_AUGER = 25;
const int RELAY_AGITATOR = 26;
const int RELAY_MIXER = 16;
const int RELAY_WATER = 17;
const int LEVEL_SWITCH = 27;     // input pull-up, LOW=active
const int WASH_STANDBY_PIN = 23; // input pull-up, LOW=active
const int WASH_DISPENSE_PIN = 5; // input pull-up, LOW=active
```
## MQTT Integration

The system publishes status to MQTT broker at `home/milk_machine/` topics:
- `state`: Current system state (IDLE, MIXING, POST_MIX, PERIODIC_MIX, WASH_STANDBY, FAULT)
- `level`: Milk level switch state (LOW/HIGH)
- `uptime`: System uptime in seconds
- `heap`: Free heap memory in bytes
- `fault_count`: Number of faults in current window
- `wash_standby`: Wash standby switch state (ON/OFF)
- `wash_dispense`: Wash dispense switch state (ON/OFF)

Commands can be sent to `home/milk_machine/command`:
- `force_mix`: Start a mixing cycle (only when IDLE)
- `stop`: Stop current mixing (only when MIXING or PERIODIC_MIX)

Home Assistant auto-discovery is supported for sensors and control switch.

## Wash Mode

The system includes a wash mode for cleaning the machine:
- **Wash Standby Switch** (GPIO23): Toggle to enter/exit wash mode
- **Wash Dispense Switch** (GPIO5): Activate water solenoid while in wash mode

When wash mode is active:
- All mixing operations are halted
- Water solenoid can be controlled independently
- System remains in WASH_STANDBY state until standby switch is released

## LCD Display

The system includes an RGB LCD display (DFRobot RGBLCD1602) showing the current system state:
- **IDLE**: Green backlight
- **MIXING/PERIODIC**: Orange backlight
- **WASH_STANDBY**: Blue backlight
- **FAULT**: Red backlight

Connected via I2C (standard pins SDA=21, SCL=22).

## Web-Based OTA Updates

The system provides a modern web interface for firmware updates at `http://[device-ip]/`:

### Features
- **Web Upload Form**: Simple browser-based firmware upload
- **Progress Tracking**: Real-time upload progress with visual feedback
- **Automatic Reboot**: Device restarts automatically after successful update
- **Safety**: Upload only works when connected to WiFi

### How to Use
1. Open web browser and navigate to `http://[milk-machine-ip]/`
2. Select a firmware `.bin` file from your computer
3. Click "Upload Firmware"
4. Wait for upload to complete and device to reboot

### Security Notes
- No authentication required (intended for local network use)
- Only accepts `.bin` files
- Device must be connected to WiFi
- ArduinoOTA remains available as backup method

## Architecture Overview

The milk mixing machine uses a modern, modular C++ architecture with the following components:

- **MilkMachineApp**: Main application coordinator
- **HardwareController**: Relay and pin management
- **InputManager**: Debounced input processing with event callbacks
- **StateMachine**: Event-driven state management
- **LCDDisplay**: Visual status display with color coding
- **MQTTManager**: Home Assistant integration and remote monitoring
- **SystemMonitor**: Health checks and fault management
- **Logger**: Multi-output logging system

### Event-Driven Design

Components communicate through events rather than direct coupling:
- Input events trigger state transitions
- State changes update displays and publish MQTT messages
- Observer pattern enables flexible component interactions

## System Operation

### Automatic Mixing Cycle
1) Trigger: Level switch active (LOW) -> start full cycle (auger+agitator+mixer+water).
2) When level restores (HIGH), transition to Post-Mix.
3) Post-Mix: mixer-only for 5 seconds.
4) Return to Idle.

### Periodic Mixing (Idle)
- Every 5 minutes, mixer-only for 5 seconds to keep milk stirred.

### ESP32 Board
- Board: FireBeetle ESP32
- Static IP: 192.168.1.16
- WiFi: SSID "WiFi", password "Gliders1!"