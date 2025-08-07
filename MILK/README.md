# Milk Mixer Control System

## Project Overview
Automated milk mixing system using ESP32 with MQTT monitoring, automatic operation based on level switch, and Home Assistant integration. Production-ready system with bulletproof reliability and comprehensive data tracking.

## Hardware Configuration

### ESP32 Board
- **Board**: FireBeetle ESP32
- **Static IP**: 192.168.1.16
- **WiFi**: "WiFi" network with password "Gliders1!"

### Pin Assignments
```cpp
const int RELAY_AUGER = 25;      // Powder auger relay (active LOW)
const int RELAY_AGITATOR = 26;   // Powder agitator relay (active LOW)
const int RELAY_MIXER = 16;      // Liquid mixer relay (active LOW)
const int RELAY_WATER = 17;      // Water solenoid relay (active LOW)
const int LEVEL_SWITCH = 12;     // Pressure switch input (INPUT_PULLUP)
#define WASH_STANDBY_PIN 23      // Wash mode switch (INPUT_PULLUP)
#define WASH_DISPENSE_PIN 5      // Water dispense switch (INPUT_PULLUP)
```

### LCD Display
- **Type**: DFRobot RGB LCD 1602
- **I2C Address**: 0x2D
- **Size**: 16x2 characters
- **Colors**: Blue (idle), Green (mixing), Red (error), Cyan (wash)

## System Operation

### Automatic Mixing Cycle
1. **Trigger**: Level switch activates (water level drops, pin goes LOW)
2. **Mixing Phase**: All relays activate (auger, agitator, mixer, water)
3. **Level Restoration**: Level switch releases (water level restored, pin goes HIGH)
4. **Post-Mixing**: Mixer only runs for 5 seconds
5. **Return to Idle**: System ready for next cycle

### Periodic Mixing (Idle Mode)
- **Frequency**: Every 5 minutes
- **Duration**: 5 seconds
- **Purpose**: Keep milk stirred to prevent separation
- **Components**: Mixer only (no auger, agitator, or water)

### Wash Mode
- **Activation**: WASH_STANDBY_PIN held LOW
- **Operation**: Manual water control via WASH_DISPENSE_PIN
- **Safety**: 30-second timeout on water activation
- **Exit**: Release WASH_STANDBY_PIN

### Error Handling
- **Timeout Error**: Mixing exceeds 60 seconds
- **Level Switch Error**: Switch stuck for 60+ seconds during mixing
- **Recovery**: 5-minute retry delay, automatic return to idle

## MQTT Integration

### Broker Configuration
```cpp
const char* mqtt_server = "192.168.1.3";
const int mqtt_port = 1883;
const char* mqtt_user = "richowen";
const char* mqtt_password = "p";
```

### MQTT Topics
```
milk_mixer/status              - System state (idle/mixing/post_mixing/wash_mode/error_*)
milk_mixer/error               - Error messages
milk_mixer/available           - Online/offline status
milk_mixer/data/total_mixes    - Lifetime mix count (persistent)
milk_mixer/data/session_mixes  - Mixes since boot
milk_mixer/data/uptime_hours   - System uptime in hours
milk_mixer/data/error_count    - Total error count (persistent)
milk_mixer/data/last_mix       - Timestamp of last mix
```

### Data Reporting
- **Initial**: Sent immediately on MQTT connection
- **Periodic**: Every 60 seconds
- **Persistent**: Total mixes, error count stored in ESP32 flash

## Home Assistant Configuration

### YAML Configuration
```yaml
mqtt:
  sensor:
    - name: "Milk Mixer Status"
      unique_id: "milk_mixer_status"
      state_topic: "milk_mixer/status"
      availability_topic: "milk_mixer/available"
      icon: "mdi:blender"
      
    - name: "Milk Mixer Total Mixes"
      unique_id: "milk_mixer_total_mixes"
      state_topic: "milk_mixer/data/total_mixes"
      availability_topic: "milk_mixer/available"
      icon: "mdi:counter"
      
    - name: "Milk Mixer Session Mixes"
      unique_id: "milk_mixer_session_mixes"
      state_topic: "milk_mixer/data/session_mixes"
      availability_topic: "milk_mixer/available"
      icon: "mdi:numeric"
      
    - name: "Milk Mixer Uptime Hours"
      unique_id: "milk_mixer_uptime_hours"
      state_topic: "milk_mixer/data/uptime_hours"
      availability_topic: "milk_mixer/available"
      unit_of_measurement: "h"
      device_class: "duration"
      icon: "mdi:clock"
      
    - name: "Milk Mixer Error Count"
      unique_id: "milk_mixer_error_count"
      state_topic: "milk_mixer/data/error_count"
      availability_topic: "milk_mixer/available"
      icon: "mdi:alert-circle"
      
    - name: "Milk Mixer Last Mix Time"
      unique_id: "milk_mixer_last_mix_time"
      state_topic: "milk_mixer/data/last_mix"
      availability_topic: "milk_mixer/available"
      icon: "mdi:clock-outline"

  binary_sensor:
    - name: "Milk Mixer Online"
      unique_id: "milk_mixer_online"
      state_topic: "milk_mixer/available"
      payload_on: "online"
      payload_off: "offline"
      device_class: "connectivity"
      
    - name: "Milk Mixer Active"
      unique_id: "milk_mixer_active"
      state_topic: "milk_mixer/status"
      availability_topic: "milk_mixer/available"
      payload_on: "mixing"
      payload_off: "idle"
      device_class: "running"
      
    - name: "Milk Mixer Error"
      unique_id: "milk_mixer_error"
      state_topic: "milk_mixer/status"
      availability_topic: "milk_mixer/available"
      value_template: "{{ 'ON' if 'error' in value else 'OFF' }}"
      device_class: "problem"
```

## Development Environment

### PlatformIO Configuration
```ini
[env:firebeetle32]
platform = espressif32
board = firebeetle32
framework = arduino
lib_deps =
    knolleary/PubSubClient
    dfrobot/DFRobot_RGBLCD1602
monitor_speed = 115200
build_flags = 
    -D MQTT_MAX_PACKET_SIZE=512
upload_protocol = espota
upload_port = 192.168.1.16
upload_flags = 
    --host_port=3232
```

### Required Libraries
- **PubSubClient**: MQTT communication
- **DFRobot_RGBLCD1602**: LCD display control
- **Preferences**: ESP32 flash storage
- **WiFi**: Network connectivity
- **ArduinoOTA**: Over-the-air updates

## Code Architecture

### State Machine
```cpp
enum SystemState {
    IDLE,        // Waiting for level switch or periodic mixing
    MIXING,      // Full mixing cycle active
    POST_MIXING, // Final mixer-only phase
    ERROR,       // Error state with retry timer
    WASH         // Manual wash mode
};
```

### Key Functions
- `initializeData()`: Load persistent data from flash
- `reportData()`: Send all metrics via MQTT
- `startMixing()`: Begin automatic mixing cycle
- `stopMixing()`: End cycle and increment counters
- `handleError()`: Error processing and recovery
- `timeElapsed()`: Overflow-safe timing function

### Safety Features
- **Debounced inputs**: 1-second debounce on level switch
- **Timeout protection**: 60-second mixing timeout
- **OTA blocking**: Prevents updates during active operations
- **Watchdog monitoring**: Detects stuck level switch
- **Memory management**: Fixed char buffers, no heap allocation

## Troubleshooting

### Common Issues
1. **No MQTT data**: Check `lastDataReport` initialization
2. **WiFi connection fails**: Verify SSID/password and signal strength
3. **Level switch issues**: Check debounce timing and wiring
4. **OTA upload fails**: Ensure device is idle and IP is correct

### Debug Output
Enable serial monitoring at 115200 baud for detailed logging:
- WiFi connection status
- MQTT connection attempts
- Data reporting with values
- State transitions
- Error conditions

### Build Issues
- **SPIFFS error**: Ensure `data/` directory exists
- **Library missing**: Install required dependencies
- **Upload fails**: Check OTA configuration and device status

## Production Notes

### Reliability Features
- **Automatic operation**: No manual intervention required
- **Network independence**: Core functions work without WiFi/MQTT
- **Persistent storage**: Counters survive power cycles
- **Error recovery**: Automatic retry after failures
- **Overflow protection**: Safe operation beyond 49 days uptime

### Maintenance
- **Monitor error count**: Indicates hardware issues
- **Check uptime**: System stability metric
- **Verify mix counts**: Production tracking
- **Update firmware**: Use OTA when system is idle

### Performance
- **Response time**: <1 second level switch detection
- **Mixing accuracy**: Consistent cycle timing
- **Network efficiency**: Minimal MQTT traffic
- **Memory usage**: Fixed allocation, no fragmentation

## Future Enhancements

### Potential Additions
- **Temperature monitoring**: Add DS18B20 sensor
- **Flow rate measurement**: Water flow sensor
- **Recipe management**: Multiple mixing profiles
- **Maintenance scheduling**: Time-based alerts
- **Remote diagnostics**: Enhanced error reporting

### Code Modifications
- All timing constants are configurable
- MQTT topics easily customizable
- Pin assignments clearly defined
- Modular function structure for easy extension

## Contact Information
This system is designed for production use with bulletproof reliability. All network functions are optional - the core mixing operation will continue even without WiFi/MQTT connectivity.
