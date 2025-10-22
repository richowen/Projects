#ifndef INTERFACES_H
#define INTERFACES_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ========================================
// BASE INTERFACES FOR DEPENDENCY INJECTION
// ========================================

/**
 * @brief Interface for WiFi connectivity management
 */
class IWiFiManager {
public:
    virtual ~IWiFiManager() = default;

    /**
     * @brief Initialize WiFi connection
     * @return true if connection successful, false otherwise
     */
    virtual bool begin() = 0;

    /**
     * @brief Check if WiFi is connected
     * @return true if connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get current WiFi signal strength (0-3)
     * @return signal level
     */
    virtual uint8_t getSignalStrength() const = 0;

    /**
     * @brief Get local IP address as string
     * @return IP address string
     */
    virtual String getIPAddress() const = 0;

    /**
     * @brief Attempt to reconnect WiFi
     * @return true if reconnected successfully
     */
    virtual bool reconnect() = 0;

    /**
     * @brief Update WiFi manager (call in main loop)
     */
    virtual void update() = 0;
};

/**
 * @brief Interface for Home Assistant API communication
 */
class IHAClient {
public:
    virtual ~IHAClient() = default;

    /**
     * @brief Initialize HA client with credentials
     * @param url HA base URL
     * @param token Long-lived access token
     * @return true if initialized successfully
     */
    virtual bool begin(const char* url, const char* token) = 0;

    /**
     * @brief Send command to HA entity
     * @param entityId Target entity ID
     * @param service Service to call (e.g., "turn_on", "toggle")
     * @param data Optional JSON data payload
     * @return true if command sent successfully
     */
    virtual bool sendCommand(const char* entityId, const char* service, JsonDocument* data = nullptr) = 0;

    /**
     * @brief Check if HA connection is available
     * @return true if HA is reachable
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get last error message
     * @return error message string
     */
    virtual String getLastError() const = 0;
};

/**
 * @brief Interface for input device management (buttons, switches)
 */
class IInputManager {
public:
    virtual ~IInputManager() = default;

    /**
     * @brief Initialize input devices
     * @return true if initialized successfully
     */
    virtual bool begin() = 0;

    /**
     * @brief Update input states (call in main loop)
     */
    virtual void update() = 0;

    /**
     * @brief Check if a button was pressed (momentary)
     * @param buttonIndex Button index
     * @return true if pressed since last check
     */
    virtual bool isButtonPressed(uint8_t buttonIndex) = 0;

    /**
     * @brief Check if a switch state changed
     * @param switchIndex Switch index
     * @return true if state changed since last check
     */
    virtual bool hasSwitchChanged(uint8_t switchIndex) = 0;

    /**
     * @brief Get current switch state
     * @param switchIndex Switch index
     * @return true if switch is ON
     */
    virtual bool getSwitchState(uint8_t switchIndex) const = 0;

    /**
     * @brief Get button name for display purposes
     * @param buttonIndex Button index
     * @return button name string
     */
    virtual const char* getButtonName(uint8_t buttonIndex) const = 0;

    /**
     * @brief Get total number of buttons/switches
     * @return count
     */
    virtual uint8_t getInputCount() const = 0;

    /**
     * @brief Get button state structure (for advanced usage)
     * @param index Button index
     * @return pointer to button state (nullptr if invalid index)
     */
    virtual const void* getButtonState(uint8_t index) const = 0;
};

/**
 * @brief Interface for sensor management (potentiometer, etc.)
 */
class ISensorManager {
public:
    virtual ~ISensorManager() = default;

    /**
     * @brief Initialize sensors
     * @return true if initialized successfully
     */
    virtual bool begin() = 0;

    /**
     * @brief Update sensor readings (call in main loop)
     */
    virtual void update() = 0;

    /**
     * @brief Get current temperature setpoint
     * @return temperature in °C
     */
    virtual int getTemperatureSetpoint() const = 0;

    /**
     * @brief Check if temperature setpoint changed
     * @return true if changed since last check
     */
    virtual bool hasTemperatureChanged() = 0;

    /**
     * @brief Get raw ADC reading for debugging
     * @return ADC value
     */
    virtual int getRawADCReading() const = 0;
};

/**
 * @brief Interface for configuration management
 */
class IConfigManager {
public:
    virtual ~IConfigManager() = default;

    /**
     * @brief Load configuration from storage
     * @return true if loaded successfully
     */
    virtual bool load() = 0;

    /**
     * @brief Save current configuration
     * @return true if saved successfully
     */
    virtual bool save() = 0;

    /**
     * @brief Get WiFi SSID
     * @return SSID string
     */
    virtual const char* getWiFiSSID() const = 0;

    /**
     * @brief Get WiFi password
     * @return password string
     */
    virtual const char* getWiFiPassword() const = 0;

    /**
     * @brief Get HA URL
     * @return URL string
     */
    virtual const char* getHAURL() const = 0;

    /**
     * @brief Get HA token
     * @return token string
     */
    virtual const char* getHAToken() const = 0;

    /**
     * @brief Get temperature range
     * @param minTemp Output parameter for minimum temperature
     * @param maxTemp Output parameter for maximum temperature
     */
    virtual void getTemperatureRange(int& minTemp, int& maxTemp) const = 0;

    /**
     * @brief Get pin configuration
     * @param pinType Type of pin (e.g., "display_cs", "button_1")
     * @return pin number
     */
    virtual int getPin(const char* pinType) const = 0;

    /**
     * @brief Get timing configuration
     * @param timingType Type of timing (e.g., "debounce_delay", "display_timeout")
     * @return timing value in milliseconds
     */
    virtual unsigned long getTiming(const char* timingType) const = 0;

    /**
     * @brief Get entity ID for a given function
     * @param entityType Type of entity (e.g., "ac_unit", "lights")
     * @return entity ID string
     */
    virtual const char* getEntityId(const char* entityType) const = 0;

    /**
     * @brief Check if debug mode is enabled
     * @return true if debug mode
     */
    virtual bool isDebugMode() const = 0;

    /**
     * @brief Get WiFi connection timeout
     * @return timeout in milliseconds
     */
    virtual unsigned long getWiFiTimeout() const = 0;

    /**
     * @brief Get WiFi retry interval
     * @return retry interval in milliseconds
     */
    virtual unsigned long getWiFiRetryInterval() const = 0;

    /**
     * @brief Get debounce delay for inputs
     * @return delay in milliseconds
     */
    virtual unsigned long getDebounceDelay() const = 0;

    /**
     * @brief Get ADC maximum value
     * @return ADC max value
     */
    virtual int getADCMaxValue() const = 0;

    /**
     * @brief Get potentiometer stability duration
     * @return stability time in milliseconds
     */
    virtual unsigned long getPotStabilityDuration() const = 0;

    /**
     * @brief Get PC shutdown hold time
     * @return hold time in milliseconds
     */
    virtual unsigned long getPCShutdownHoldTime() const = 0;

    /**
     * @brief Get Plex on hold time
     * @return hold time in milliseconds
     */
    virtual unsigned long getPlexOnHoldTime() const = 0;
};

/**
 * @brief Interface for display management
 */
class IDisplayManager {
public:
    virtual ~IDisplayManager() = default;

    /**
     * @brief Initialize display
     * @return true if initialized successfully
     */
    virtual bool begin() = 0;

    /**
     * @brief Update display state (call in main loop)
     */
    virtual void update() = 0;

    /**
     * @brief Show icon
     * @param iconIndex Icon to display
     * @param duration Display duration in ms (0 = until changed)
     */
    virtual void showIcon(uint8_t iconIndex, uint16_t duration = 1000) = 0;

    /**
     * @brief Show action icon based on action name
     * @param actionName Name of the action
     */
    virtual void showActionIcon(const char* actionName) = 0;

    /**
     * @brief Show success feedback
     * @param duration Display duration in ms
     */
    virtual void showSuccess(uint16_t duration = 1000) = 0;

    /**
     * @brief Show error feedback
     * @param duration Display duration in ms
     */
    virtual void showError(uint16_t duration = 1500) = 0;

    /**
     * @brief Show progress bar
     * @param percent Progress percentage (0-100)
     * @param style Progress bar style
     */
    virtual void showProgress(uint8_t percent, uint8_t style = 0) = 0;

    /**
     * @brief Show temperature
     * @param temp Temperature value
     */
    virtual void showTemperature(int temp) = 0;

    /**
     * @brief Clear display
     */
    virtual void clear() = 0;

    /**
     * @brief Flash display
     * @param duration Flash duration in ms
     */
    virtual void flash(uint16_t duration = 100) = 0;

    /**
     * @brief Set WiFi signal strength indicator
     * @param level Signal level (0-3)
     */
    virtual void setWiFiSignal(uint8_t level) = 0;
};

/**
 * @brief Interface for logging system
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    /**
     * @brief Log debug message
     * @param message Message to log
     */
    virtual void debug(const char* message) = 0;

    /**
     * @brief Log info message
     * @param message Message to log
     */
    virtual void info(const char* message) = 0;

    /**
     * @brief Log warning message
     * @param message Message to log
     */
    virtual void warning(const char* message) = 0;

    /**
     * @brief Log error message
     * @param message Message to log
     */
    virtual void error(const char* message) = 0;

    /**
     * @brief Log formatted message with level
     * @param level Log level
     * @param format printf-style format string
     * @param ... format arguments
     */
    virtual void logf(const char* level, const char* format, ...) = 0;
};

#endif // INTERFACES_H