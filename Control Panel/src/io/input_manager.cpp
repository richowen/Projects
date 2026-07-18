#include "input_manager.h"

// Button names for debug output (matching original)
static const char* BUTTON_NAMES[] = {
    "AC Power", "AC Bypass", "PC Shutdown", "Lights", "Immersion",
    "Extra 1", "Plex On", "Extra 3", "Extra 4", "Extra 5"
};

// ========================================
// CONSTRUCTOR
// ========================================

InputManager::InputManager(IConfigManager* config, ILogger* logger)
    : _config(config), _logger(logger) {

    // Initialize button states
    for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
        _buttons[i].pin = 0;
        _buttons[i].lastState = HIGH;
        _buttons[i].currentState = HIGH;
        _buttons[i].lastDebounceTime = 0;
        _buttons[i].entityId[0] = '\0';
        _buttons[i].service[0] = '\0';
        _buttons[i].type = MOMENTARY_BUTTON;
        _buttonPressedFlags[i] = false;
        _switchChangedFlags[i] = false;
    }
}


// ========================================
// IINPUTMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool InputManager::begin() {
    _logger->info("Input Manager: Initializing...");

    // Configure button/switch setup from runtime-editable ConfigManager
    // (falls back to config.h defaults if no saved config exists in NVS)
    for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
        char pinKey[16];
        snprintf(pinKey, sizeof(pinKey), "button_%d", i);

        InputType type = (_config->getInputType(i) == 1) ? TOGGLE_SWITCH : MOMENTARY_BUTTON;
        configureInput(i, _config->getPin(pinKey), _config->getInputEntityId(i),
                       _config->getInputService(i), type);
    }

    initializePins();

    _logger->info("Input Manager: Initialization complete");
    return true;
}


void InputManager::update() {
    for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
        updateDebouncing(i);
    }
}

bool InputManager::isButtonPressed(uint8_t buttonIndex) {
    if (buttonIndex >= MAX_BUTTONS) return false;

    bool wasPressed = _buttonPressedFlags[buttonIndex];
    _buttonPressedFlags[buttonIndex] = false;  // Clear flag after reading
    return wasPressed;
}

bool InputManager::hasSwitchChanged(uint8_t switchIndex) {
    if (switchIndex >= MAX_BUTTONS) return false;

    bool changed = _switchChangedFlags[switchIndex];
    _switchChangedFlags[switchIndex] = false;  // Clear flag after reading
    return changed;
}

bool InputManager::getSwitchState(uint8_t switchIndex) const {
    if (switchIndex >= MAX_BUTTONS) return false;
    return _buttons[switchIndex].currentState == LOW;  // LOW = ON position
}

const char* InputManager::getButtonName(uint8_t buttonIndex) const {
    return getButtonNameInternal(buttonIndex);
}

uint8_t InputManager::getInputCount() const {
    return MAX_BUTTONS;
}

bool InputManager::configureInput(uint8_t index, int pin, const char* entityId, const char* service, InputType type) {
    if (index >= MAX_BUTTONS) return false;

    _buttons[index].pin = pin;
    strncpy(_buttons[index].entityId, entityId ? entityId : "", sizeof(_buttons[index].entityId) - 1);
    _buttons[index].entityId[sizeof(_buttons[index].entityId) - 1] = '\0';
    strncpy(_buttons[index].service, service ? service : "", sizeof(_buttons[index].service) - 1);
    _buttons[index].service[sizeof(_buttons[index].service) - 1] = '\0';
    _buttons[index].type = type;
    _buttons[index].lastState = HIGH;
    _buttons[index].currentState = HIGH;
    _buttons[index].lastDebounceTime = 0;

    _logger->logf("DEBUG", "Input Manager: Configured %s %d - Pin: %d, Entity: %s, Type: %s",
                  type == MOMENTARY_BUTTON ? "Button" : "Switch",
                  index, pin, entityId,
                  type == MOMENTARY_BUTTON ? "Momentary" : "Toggle");

    return true;
}

bool InputManager::reloadInputConfig(uint8_t index) {
    if (index >= MAX_BUTTONS) return false;

    InputType type = (_config->getInputType(index) == 1) ? TOGGLE_SWITCH : MOMENTARY_BUTTON;
    strncpy(_buttons[index].entityId, _config->getInputEntityId(index), sizeof(_buttons[index].entityId) - 1);
    _buttons[index].entityId[sizeof(_buttons[index].entityId) - 1] = '\0';
    strncpy(_buttons[index].service, _config->getInputService(index), sizeof(_buttons[index].service) - 1);
    _buttons[index].service[sizeof(_buttons[index].service) - 1] = '\0';
    _buttons[index].type = type;

    _logger->logf("INFO", "Input Manager: Live-reloaded input %d - Entity: %s, Service: %s, Type: %s",
                  index, _buttons[index].entityId, _buttons[index].service,
                  type == MOMENTARY_BUTTON ? "Momentary" : "Toggle");

    return true;
}


const InputManager::ButtonState* InputManager::getButtonState(uint8_t index) const {
    if (index >= MAX_BUTTONS) return nullptr;
    return &_buttons[index];
}

// ========================================
// PRIVATE METHODS
// ========================================

void InputManager::initializePins() {
    _logger->info("Input Manager: Configuring pins...");

    for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
        if (_buttons[i].pin > 0) {
            pinMode(_buttons[i].pin, INPUT_PULLUP);
            _buttons[i].lastState = digitalRead(_buttons[i].pin);
            _buttons[i].currentState = _buttons[i].lastState;

            _logger->logf("DEBUG", "Input Manager: %s (GPIO %d) initialized",
                          getButtonNameInternal(i), _buttons[i].pin);
        }
    }
}

void InputManager::updateDebouncing(uint8_t index) {
    if (index >= MAX_BUTTONS || _buttons[index].pin <= 0) return;

    int reading = digitalRead(_buttons[index].pin);

    // Check if state changed
    if (reading != _buttons[index].lastState) {
        _buttons[index].lastDebounceTime = millis();
    }

    // If enough time has passed, consider it a valid state change
    if ((millis() - _buttons[index].lastDebounceTime) > _config->getDebounceDelay()) {
        if (reading != _buttons[index].currentState) {
            _buttons[index].currentState = reading;

            if (_buttons[index].type == MOMENTARY_BUTTON) {
                handleMomentaryButton(index);
            } else {
                handleToggleSwitch(index);
            }
        }
    }

    _buttons[index].lastState = reading;
}

void InputManager::handleMomentaryButton(uint8_t index) {
    // Only trigger on press (HIGH to LOW transition)
    if (_buttons[index].currentState == LOW) {
        _buttonPressedFlags[index] = true;

        _logger->logf("INFO", "Input Manager: %s pressed (GPIO %d)",
                      getButtonNameInternal(index), _buttons[index].pin);
    }
}

void InputManager::handleToggleSwitch(uint8_t index) {
    _switchChangedFlags[index] = true;

    const char* position = (_buttons[index].currentState == LOW) ? "ON" : "OFF";
    _logger->logf("INFO", "Input Manager: %s switched to %s position (GPIO %d)",
                  getButtonNameInternal(index), position, _buttons[index].pin);
}

const char* InputManager::getButtonNameInternal(uint8_t index) const {
    if (index < MAX_BUTTONS) {
        return BUTTON_NAMES[index];
    }
    return "Unknown";
}