#include "control_panel.h"
#include "io/input_manager.h"
#include <ArduinoJson.h>

// ========================================
// CONSTRUCTOR
// ========================================

ControlPanel::ControlPanel(IConfigManager* config, ILogger* logger, IWiFiManager* wifiManager,
                         IHAClient* haClient, IInputManager* inputManager,
                         ISensorManager* sensorManager, DisplayManager* displayManager)
    : _config(config), _logger(logger), _wifiManager(wifiManager), _haClient(haClient),
      _inputManager(inputManager), _sensorManager(sensorManager), _displayManager(displayManager),
      _pcShutdownHoldStart(0), _pcShutdownInProgress(false),
      _plexOnHoldStart(0), _plexOnInProgress(false) {
}

// ========================================
// PUBLIC METHODS
// ========================================

bool ControlPanel::begin() {
    _logger->info("Control Panel: Starting initialization...");

    if (!initializeComponents()) {
        _logger->error("Control Panel: Component initialization failed");
        return false;
    }

    showBootSequence();

    _logger->info("Control Panel: Initialization complete");
    return true;
}

void ControlPanel::update() {
    // Update all managers
    _wifiManager->update();
    _inputManager->update();
    _sensorManager->update();
    _displayManager->update();

    // Process events
    processInputs();
    processSensors();
    updateHoldProgress();
}

void ControlPanel::handleButtonPress(uint8_t buttonIndex) {
    // Special handling for specific physical button positions (hold-to-activate safety buttons)
    // These are tied to the physical button's index/wiring, not its (user-editable) entity/service
    if (buttonIndex == 2) {        // PC Shutdown button position
        handlePCShutdownButton(buttonIndex);
    } else if (buttonIndex == 6) { // Plex On button position
        handlePlexOnButton(buttonIndex);
    } else {
        handleNormalButton(buttonIndex);
    }
}


void ControlPanel::handleSwitchChange(uint8_t switchIndex) {
    handleToggleSwitch(switchIndex);
}

void ControlPanel::handleTemperatureChange() {
    int temperature = _sensorManager->getTemperatureSetpoint();

    _logger->logf("INFO", "Control Panel: Sending stable temperature %d°C to Home Assistant", temperature);

    // Send temperature setpoint to HA (display is already updated in processSensors)
    StaticJsonDocument<128> data;
    data["value"] = temperature;

    sendHACommand(_config->getEntityId("ac_temp"), "set_value", &data);
}

void ControlPanel::handleBrightnessChange() {
    int brightness = _sensorManager->getBrightnessPercentage();
    
    _logger->logf("INFO", "Control Panel: Sending stable brightness %d%% to Home Assistant", brightness);
    
    // Send brightness command to HA (display is already updated in processSensors)
    StaticJsonDocument<128> data;
    data["brightness_pct"] = brightness;
    
    sendHACommand(_config->getEntityId("lights_brightness"), "turn_on", &data);
}

// ========================================
// PRIVATE METHODS
// ========================================

bool ControlPanel::initializeComponents() {
    // Initialize HA Client
    if (!_haClient->begin(_config->getHAURL(), _config->getHAToken())) {
        return false;
    }

    // Initialize WiFi
    if (!_wifiManager->begin()) {
        return false;
    }

    // Initialize input devices
    if (!_inputManager->begin()) {
        return false;
    }

    // Initialize sensors
    if (!_sensorManager->begin()) {
        return false;
    }

    // Initialize display (DisplayManager doesn't have begin method, init is called in constructor)
    // Display is initialized when MD_Parola is created in main

    return true;
}

void ControlPanel::showBootSequence() {
    #if ENABLE_BOOT_ANIMATION
        _logger->info("Control Panel: Starting boot animation...");
        // Boot animation is handled by display manager
        delay(1500);
    #else
        _displayManager->showIcon(ICON_CHECK, 500);
        delay(500);
    #endif

    // Show ready state
    #if ENABLE_IDLE_ANIMATIONS
        // Idle animations enabled
    #endif

    // Set WiFi signal strength for display
    _displayManager->setWiFiSignal(_wifiManager->getSignalStrength());
}

void ControlPanel::processInputs() {
    // Check for button presses
    for (uint8_t i = 0; i < _inputManager->getInputCount(); i++) {
        if (_inputManager->isButtonPressed(i)) {
            handleButtonPress(i);
        }

        if (_inputManager->hasSwitchChanged(i)) {
            handleSwitchChange(i);
        }
    }
}

void ControlPanel::processSensors() {
    if (_sensorManager->isBrightnessMode()) {
        // Brightness control mode
        static int lastDisplayedBrightness = -999;
        int currentBrightness = _sensorManager->getBrightnessPercentage();
        
        if (currentBrightness != lastDisplayedBrightness) {
            _displayManager->showBrightness(currentBrightness);
            lastDisplayedBrightness = currentBrightness;
            _logger->logf("DEBUG", "Control Panel: Displaying live brightness: %d%%", currentBrightness);
        }
        
        if (_sensorManager->hasBrightnessChanged()) {
            handleBrightnessChange();
        }
    } else {
        // Temperature control mode
        static int lastDisplayedTemp = -999;
        int currentTemp = _sensorManager->getTemperatureSetpoint();
        
        if (currentTemp != lastDisplayedTemp) {
            _displayManager->showTemperature(currentTemp);
            lastDisplayedTemp = currentTemp;
            _logger->logf("DEBUG", "Control Panel: Displaying live temp: %d°C", currentTemp);
        }
        
        if (_sensorManager->hasTemperatureChanged()) {
            handleTemperatureChange();
        }
    }
}

void ControlPanel::updateHoldProgress() {
    // Update PC shutdown progress
    if (_pcShutdownInProgress) {
        // Check if button is still being held down
        const void* voidState = _inputManager->getButtonState(2); // PC shutdown is button index 2
        const InputManager::ButtonState* buttonState = static_cast<const InputManager::ButtonState*>(voidState);
        
        if (buttonState && buttonState->currentState == HIGH) {
            // Button was released - cancel countdown
            _logger->info("Control Panel: PC shutdown button released - cancelling countdown");
            _pcShutdownInProgress = false;
            _displayManager->clear();
            return;
        }
        
        unsigned long elapsed = millis() - _pcShutdownHoldStart;
        uint8_t progress = (uint8_t)((elapsed * 100) / _config->getPCShutdownHoldTime());

        if (progress > 100) progress = 100;
        _displayManager->showProgress(progress, PROGRESS_SPIRAL);
        
        _logger->logf("DEBUG", "PC shutdown hold progress: %d%%, button state: %s",
                      progress, buttonState ? (buttonState->currentState == LOW ? "HELD" : "RELEASED") : "UNKNOWN");

        // Check if hold time completed
        if (elapsed >= _config->getPCShutdownHoldTime()) {
            _logger->info("Control Panel: PC shutdown hold completed - button held for full duration");
            sendHACommand(_config->getEntityId("pc_shutdown"), "trigger");
            _pcShutdownInProgress = false;
            _displayManager->clear();
        }
    }

    // Update Plex on progress
    if (_plexOnInProgress) {
        // Check if button is still being held down
        const void* voidState = _inputManager->getButtonState(6); // Plex on is button index 6
        const InputManager::ButtonState* buttonState = static_cast<const InputManager::ButtonState*>(voidState);
        
        if (buttonState && buttonState->currentState == HIGH) {
            // Button was released - cancel countdown
            _logger->info("Control Panel: Plex on button released - cancelling countdown");
            _plexOnInProgress = false;
            _displayManager->clear();
            return;
        }
        
        unsigned long elapsed = millis() - _plexOnHoldStart;
        uint8_t progress = (uint8_t)((elapsed * 100) / _config->getPlexOnHoldTime());

        if (progress > 100) progress = 100;
        _displayManager->showProgress(progress, PROGRESS_EXPAND_SQUARE);
        
        _logger->logf("DEBUG", "Plex on hold progress: %d%%, button state: %s",
                      progress, buttonState ? (buttonState->currentState == LOW ? "HELD" : "RELEASED") : "UNKNOWN");

        // Check if hold time completed
        if (elapsed >= _config->getPlexOnHoldTime()) {
            _logger->info("Control Panel: Plex on hold completed - button held for full duration");
            sendHACommand(_config->getEntityId("plex"), "trigger");
            _plexOnInProgress = false;
            _displayManager->clear();
        }
    }
}

void ControlPanel::handlePCShutdownButton(uint8_t buttonIndex) {
    if (!_pcShutdownInProgress) {
        _logger->info("Control Panel: PC shutdown button pressed - starting hold timer");
        _pcShutdownInProgress = true;
        _pcShutdownHoldStart = millis();
        
        // Don't clear - progress bar will display immediately on next update
        _logger->debug("Control Panel: PC shutdown - starting progress bar");
    }
}

void ControlPanel::handlePlexOnButton(uint8_t buttonIndex) {
    if (!_plexOnInProgress) {
        _logger->info("Control Panel: Plex on button pressed - starting hold timer");
        _plexOnInProgress = true;
        _plexOnHoldStart = millis();
        
        // Don't flash or show icon for hold-to-trigger buttons
        // Progress bar will display immediately on next update
        _logger->debug("Control Panel: Plex on - skipping flash, starting progress bar");
    }
}

void ControlPanel::handleNormalButton(uint8_t buttonIndex) {
    const void* voidState = _inputManager->getButtonState(buttonIndex);
    const InputManager::ButtonState* buttonState = static_cast<const InputManager::ButtonState*>(voidState);
    if (!buttonState) return;

    _logger->logf("INFO", "Control Panel: Normal button %s pressed", _inputManager->getButtonName(buttonIndex));

    // Visual feedback
    #if ENABLE_BUTTON_FLASH
        _displayManager->flash(50);
    #endif
    _displayManager->showActionIcon(_inputManager->getButtonName(buttonIndex));

    sendHACommand(buttonState->entityId, buttonState->service);
}

void ControlPanel::handleToggleSwitch(uint8_t switchIndex) {
    const void* voidState = _inputManager->getButtonState(switchIndex);
    const InputManager::ButtonState* buttonState = static_cast<const InputManager::ButtonState*>(voidState);
    if (!buttonState) return;

    bool isOn = _inputManager->getSwitchState(switchIndex);
    const char* service = isOn ? "turn_on" : "turn_off";

    _logger->logf("INFO", "Control Panel: Switch %s changed to %s",
                  _inputManager->getButtonName(switchIndex), isOn ? "ON" : "OFF");

    // Special handling for AC Bypass switch (physical position, index 1)
    if (switchIndex == 1) {

        // Notify sensor manager of bypass state change
        _sensorManager->setACBypassState(isOn);
        
        if (isOn) {
            // Switch ON = Turn on bypass boolean
            sendHACommand(_config->getEntityId("ac_bypass"), "turn_on");
        } else {
            // Switch OFF = Trigger bypass-off automation
            sendHACommand(_config->getEntityId("ac_automation"), "trigger");
        }
    } else {
        // Normal toggle switch
        sendHACommand(buttonState->entityId, service);
    }

    // Visual feedback
    #if ENABLE_BUTTON_FLASH
        _displayManager->flash(50);
    #endif
    _displayManager->showActionIcon(_inputManager->getButtonName(switchIndex));
}

bool ControlPanel::sendHACommand(const char* entityId, const char* service, JsonDocument* data) {
    return _haClient->sendCommand(entityId, service, data);
}