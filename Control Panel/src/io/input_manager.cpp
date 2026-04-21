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
        _buttons[i] = {0, HIGH, HIGH, 0, "", "", MOMENTARY_BUTTON};
        _buttonPressedFlags[i] = false;
        _switchChangedFlags[i] = false;
    }
}

// ========================================
// IINPUTMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool InputManager::begin() {
    _logger->info("Input Manager: Initializing...");

    // Configure default button/switch setup (matching original)
    configureInput(0, _config->getPin("button_0"), _config->getEntityId("ac_unit"), "toggle", MOMENTARY_BUTTON);
    configureInput(1, _config->getPin("button_1"), _config->getEntityId("ac_bypass"), "input_boolean", TOGGLE_SWITCH);
    configureInput(2, _config->getPin("button_2"), _config->getEntityId("pc_shutdown"), "trigger", MOMENTARY_BUTTON);
    configureInput(3, _config->getPin("button_3"), _config->getEntityId("lights"), "trigger", MOMENTARY_BUTTON);
    configureInput(4, _config->getPin("button_4"), _config->getEntityId("immersion"), "switch", TOGGLE_SWITCH);
    configureInput(5, _config->getPin("button_5"), _config->getEntityId("extra_1"), "turn_on", MOMENTARY_BUTTON);
    configureInput(6, _config->getPin("button_6"), _config->getEntityId("plex"), "trigger", MOMENTARY_BUTTON);
    configureInput(7, _config->getPin("button_7"), _config->getEntityId("extra_3"), "toggle", MOMENTARY_BUTTON);
    configureInput(8, _config->getPin("button_8"), _config->getEntityId("extra_4"), "toggle", MOMENTARY_BUTTON);
    configureInput(9, _config->getPin("button_9"), _config->getEntityId("extra_5"), "toggle", MOMENTARY_BUTTON);

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
    _buttons[index].entityId = entityId;
    _buttons[index].service = service;
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