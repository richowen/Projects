
# Control Panel Refactoring Guide

## Overview

This document describes the refactored modular architecture of the Home Control Panel project. The refactoring transforms a monolithic 773-line main.cpp into a clean, modular system optimized for AI agent collaboration and maintenance.

## Architecture

### Core Principles

1. **Single Responsibility** - Each module has one clear purpose
2. **Dependency Injection** - Components receive dependencies through constructors
3. **Interface-Based Design** - Abstract interfaces enable testability and flexibility
4. **Clear Boundaries** - Well-defined module interactions
5. **Consistent Patterns** - Standardized error handling, logging, and configuration

### Module Organization

```
Control Panel/
├── include/
│   ├── interfaces.h          # Abstract interfaces for all components
│   ├── config.h              # Hardware and feature configuration
│   └── secrets.h             # WiFi and HA credentials (gitignored)
│
├── src/
│   ├── main.cpp              # Minimal entry point (~50 lines)
│   ├── control_panel.h/cpp   # Main orchestrator
│   │
│   ├── core/                 # Foundation components
│   │   ├── config_manager.h/cpp    # Configuration access
│   │   └── logger.h/cpp            # Structured logging
│   │
│   ├── network/              # Network communication
│   │   ├── wifi_manager.h/cpp      # WiFi connectivity
│   │   └── ha_client.h/cpp         # Home Assistant API
│   │
│   ├── io/                   # Input/output management
│   │   ├── input_manager.h/cpp     # Buttons and switches
│   │   └── sensor_manager.h/cpp    # Potentiometer
│   │
│   └── display/              # Display management
│       ├── display_manager.h/cpp   # Display UX (existing)
│       └── icons.h                 # Icon definitions
```

## Component Details

### 1. ConfigManager (`src/core/config_manager.h/cpp`)

**Responsibility**: Centralized access to all configuration values

**Key Methods**:
- `getPin(type)` - Get GPIO pin numbers
- `getTiming(type)` - Get timing configurations
- `getEntityId(type)` - Get HA entity IDs
- `isDebugMode()` - Check debug mode status

**Why It Helps AI**:
- Single source of truth for configuration
- Clear method names make intent obvious
- Easy to extend with new configuration types

### 2. Logger (`src/core/logger.h/cpp`)

**Responsibility**: Structured logging with levels and timestamps

**Key Methods**:
- `debug()`, `info()`, `warning()`, `error()` - Log at different levels
- `logf()` - Printf-style formatted logging

**Why It Helps AI**:
- Consistent logging pattern across all modules
- Easy to add logging to new code
- Filterable output for debugging

### 3. WiFiManager (`src/network/wifi_manager.h/cpp`)

**Responsibility**: WiFi connectivity and reconnection logic

**Key Methods**:
- `begin()` - Initialize WiFi connection
- `isConnected()` - Check connection status
- `reconnect()` - Attempt reconnection
- `update()` - Monitor and maintain connection

**Why It Helps AI**:
- All WiFi logic isolated in one place
- Clear connection state management
- Easy to modify reconnection strategy

### 4. HAClient (`src/network/ha_client.h/cpp`)

**Responsibility**: Home Assistant API communication

**Key Methods**:
- `sendCommand()` - Send commands to HA entities
- `isConnected()` - Check HA availability
- `getLastError()` - Retrieve error details

**Why It Helps AI**:
- All HA communication in one module
- Clear API abstraction
- Easy to add new HA features

### 5. InputManager (`src/io/input_manager.h/cpp`)

**Responsibility**: Handle buttons and switches with debouncing

**Key Methods**:
- `isButtonPressed()` - Check for button presses
- `hasSwitchChanged()` - Check for switch changes
- `getSwitchState()` - Get current switch position
- `configureInput()` - Configure button/switch behavior

**Why It Helps AI**:
- All input handling logic centralized
- Debouncing handled automatically
- Easy to add new buttons

### 6. SensorManager (`src/io/sensor_manager.h/cpp`)

**Responsibility**: Read and process potentiometer values

**Key Methods**:
- `getTemperatureSetpoint()` - Get current setpoint
- `hasTemperatureChanged()` - Detect changes
- `update()` - Process readings with averaging

**Why It Helps AI**:
- All sensor logic isolated
- Clear value processing pipeline
- Easy to add new sensors

### 7. ControlPanel (`src/control_panel.h/cpp`)

**Responsibility**: Orchestrate all components and handle application logic

**Key Methods**:
- `begin()` - Initialize all components
- `update()` - Main update loop
- `handleButtonPress()` - Process button events
- `handleSwitchChange()` - Process switch events
- `handleTemperatureChange()` - Process temperature changes

**Why It Helps AI**:
- Clear application flow
- Event-driven architecture
- Easy to understand control logic

## Typical Modification Patterns

### Adding a New Button

1. **Update config.h**: Add pin and entity definitions
2. **Update InputManager**: The configureInput() call in begin() handles it automatically
3. **Update ControlPanel**: Add special handling if needed (most buttons work automatically)

### Adding a New Sensor

1. **Create new sensor class** in `src/io/`
2. **Add interface** to `interfaces.h`
3. **Instantiate in main.cpp**
4. **Add to ControlPanel** constructor and update logic

### Modifying WiFi Behavior

1. **Edit WiFiManager** class only
2. **No other files** need changes
3. **Clear isolation** of concerns

### Adding New HA Entities

1. **Update config.h**: Add entity ID defines
2. **Update ConfigManager**: Add to getEntityId() method
3. **Use existing** HAClient methods

## Debugging Guide

### Log Levels

- **DEBUG**: Detailed internal state (verbose)
- **INFO**: Normal operations and state changes
- **WARNING**: Recoverable issues
- **ERROR**: Critical failures

### Common Issues

1. **Null pointer crashes**: Check all constructors initialize pointers
2. **Configuration errors**: Verify config.h defines are correct
3. **WiFi issues**: Check WiFiManager logs for connection state
4. **HA command failures**: Check HAClient logs for API errors

## Testing Strategy

### Unit Testing (Future)

Each module can be tested independently:

```cpp
// Example: Testing WiFiManager
MockConfigManager config;
MockLogger logger;
WiFiManager wifi(&config, &logger);

// Test connection
bool result = wifi.begin();
assert(result == true);
```

### Integration Testing

The modular design allows testing component interactions:

```cpp
// Example: Testing ControlPanel with sensors
ControlPanel panel(&config, &logger, &wifi, &ha, &input, &sensor, &display);
panel.begin();
panel.update();
// Verify sensor changes trigger HA commands
```

## Future Enhancements

### Recommended Next Steps

1. **Runtime Configuration**: Load settings from EEPROM/SD card
2. **Error Recovery**: Implement automatic recovery from failures
3. **Unit Tests**: Add test framework and tests for each module
4. **Event System**: Decouple components with event bus pattern
5. **State Persistence**: Save/restore system state across reboots

### Extension Points

- **New input types**: Extend InputManager with rotary encoders, etc.
- **New communication protocols**: Add MQTT, WebSockets alongside REST API
