#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "interfaces.h"

/**
 * @brief Input device manager for buttons and switches
 *
 * Handles debouncing, state tracking, and event detection for all input devices.
 * Supports both momentary buttons and toggle switches with different behaviors.
 */
class InputManager : public IInputManager {
public:
    /**
     * @brief Input types
     */
    enum InputType {
        MOMENTARY_BUTTON,  // Press = send command once
        TOGGLE_SWITCH      // Position = send on/off based on state
    };

    /**
     * @brief Button state structure
     */
    struct ButtonState {
        int pin;
        bool lastState;
        bool currentState;
        unsigned long lastDebounceTime;
        char entityId[64];
        char service[32];
        InputType type;
    };


    /**
     * @brief Constructor
     * @param config Configuration manager reference
     * @param logger Logger reference
     */
    InputManager(IConfigManager* config, ILogger* logger);

    // IInputManager interface implementation
    bool begin() override;
    void update() override;
    bool isButtonPressed(uint8_t buttonIndex) override;
    bool hasSwitchChanged(uint8_t switchIndex) override;
    bool getSwitchState(uint8_t switchIndex) const override;
    const char* getButtonName(uint8_t buttonIndex) const override;
    uint8_t getInputCount() const override;

    /**
     * @brief Configure a button/switch
     * @param index Button index (0-9)
     * @param pin GPIO pin
     * @param entityId HA entity ID
     * @param service Service to call
     * @param type Input type
     * @return true if configured successfully
     */
    bool configureInput(uint8_t index, int pin, const char* entityId, const char* service, InputType type);

    /**
     * @brief Reload a single input's entity/service/type from ConfigManager (live update, no reboot)
     * @param index Input index (0-9)
     * @return true if reloaded successfully
     */
    bool reloadInputConfig(uint8_t index);


    /**
     * @brief Get button state structure (for advanced usage)
     * @param index Button index
     * @return pointer to button state (nullptr if invalid index)
     */
    const ButtonState* getButtonState(uint8_t index) const;

private:
    IConfigManager* _config;
    ILogger* _logger;

    static const uint8_t MAX_BUTTONS = 10;
    ButtonState _buttons[MAX_BUTTONS];
    bool _buttonPressedFlags[MAX_BUTTONS];  // Flags for momentary button presses
    bool _switchChangedFlags[MAX_BUTTONS];  // Flags for switch state changes

    /**
     * @brief Initialize button/switch pins
     */
    void initializePins();

    /**
     * @brief Update debouncing for a specific button
     * @param index Button index
     */
    void updateDebouncing(uint8_t index);

    /**
     * @brief Handle momentary button press
     * @param index Button index
     */
    void handleMomentaryButton(uint8_t index);

    /**
     * @brief Handle toggle switch change
     * @param index Switch index
     */
    void handleToggleSwitch(uint8_t index);

    /**
     * @brief Get button name from index
     * @param index Button index
     * @return button name string
     */
    const char* getButtonNameInternal(uint8_t index) const;
};

#endif // INPUT_MANAGER_H