#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include "interfaces.h"
#include "display/display_manager.h"

/**
 * @brief Main control panel orchestrator
 *
 * Coordinates all system components and manages the main application flow.
 * Handles button actions, sensor monitoring, display updates, and HA communication.
 */
class ControlPanel {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     * @param logger Logger instance
     * @param wifiManager WiFi connectivity manager
     * @param haClient Home Assistant client
     * @param inputManager Input device manager
     * @param sensorManager Sensor manager
     * @param displayManager Display manager
     */
    ControlPanel(IConfigManager* config, ILogger* logger, IWiFiManager* wifiManager,
                 IHAClient* haClient, IInputManager* inputManager,
                 ISensorManager* sensorManager, DisplayManager* displayManager);

    /**
     * @brief Initialize the control panel
     * @return true if initialization successful
     */
    bool begin();

    /**
     * @brief Main update loop (call every loop iteration)
     */
    void update();

    /**
     * @brief Handle button press events
     * @param buttonIndex Index of pressed button
     */
    void handleButtonPress(uint8_t buttonIndex);

    /**
     * @brief Handle switch state changes
     * @param switchIndex Index of changed switch
     */
    void handleSwitchChange(uint8_t switchIndex);

    /**
     * @brief Handle temperature setpoint changes
     */
    void handleTemperatureChange();

private:
    IConfigManager* _config;
    ILogger* _logger;
    IWiFiManager* _wifiManager;
    IHAClient* _haClient;
    IInputManager* _inputManager;
    ISensorManager* _sensorManager;
    DisplayManager* _displayManager;

    // Hold-to-activate state tracking
    unsigned long _pcShutdownHoldStart;
    bool _pcShutdownInProgress;
    unsigned long _plexOnHoldStart;
    bool _plexOnInProgress;

    /**
     * @brief Initialize system components
     */
    bool initializeComponents();

    /**
     * @brief Show boot sequence
     */
    void showBootSequence();

    /**
     * @brief Process all input events
     */
    void processInputs();

    /**
     * @brief Process sensor updates
     */
    void processSensors();

    /**
     * @brief Update hold-to-activate progress indicators
     */
    void updateHoldProgress();

    /**
     * @brief Handle special PC shutdown button with hold requirement
     * @param buttonIndex Button index
     */
    void handlePCShutdownButton(uint8_t buttonIndex);

    /**
     * @brief Handle special Plex on button with hold requirement
     * @param buttonIndex Button index
     */
    void handlePlexOnButton(uint8_t buttonIndex);

    /**
     * @brief Handle normal momentary button press
     * @param buttonIndex Button index
     */
    void handleNormalButton(uint8_t buttonIndex);

    /**
     * @brief Handle toggle switch state change
     * @param switchIndex Switch index
     */
    void handleToggleSwitch(uint8_t switchIndex);

    /**
     * @brief Send command to Home Assistant
     * @param entityId Target entity
     * @param service Service to call
     * @param data Optional JSON data
     * @return true if command sent successfully
     */
    bool sendHACommand(const char* entityId, const char* service, JsonDocument* data = nullptr);
};

#endif // CONTROL_PANEL_H