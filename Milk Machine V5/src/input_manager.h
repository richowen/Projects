#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "config.h"
#include "hardware_controller.h"
#include "logger.h"
#include <functional>
#include <vector>

// Input state tracking
struct InputStates {
    int levelSwitch = HIGH;
    int washStandby = HIGH;
    int washDispense = HIGH;

    unsigned long levelSwitchStableSince = 0;
    unsigned long washStandbyStableSince = 0;
    unsigned long washDispenseStableSince = 0;

    int lastLevelRead = HIGH;
    int lastWashStandbyRead = HIGH;
    int lastWashDispenseRead = HIGH;
};

// Event callback type
using InputEventCallback = std::function<void(SystemEvent)>;

class InputManager : public LogObserver {
private:
    Logger& _logger;
    HardwareController& _hardware;
    InputStates _states;
    std::vector<InputEventCallback> _callbacks;

public:
    InputManager(Logger& logger, HardwareController& hardware)
        : _logger(logger), _hardware(hardware) {
        _logger.addObserver(this);
    }

    ~InputManager() {
        _logger.removeObserver(this);
    }

    void initialize() {
        _logger.info("Initializing input manager...");

        // Initialize stable states
        _states.lastLevelRead = _hardware.readLevelSwitch();
        _states.levelSwitch = _states.lastLevelRead;
        _states.levelSwitchStableSince = millis();

        _states.lastWashStandbyRead = _hardware.readWashStandby();
        _states.washStandby = _states.lastWashStandbyRead;
        _states.washStandbyStableSince = millis();

        _states.lastWashDispenseRead = _hardware.readWashDispense();
        _states.washDispense = _states.lastWashDispenseRead;
        _states.washDispenseStableSince = millis();

        _logger.info("Input manager initialized.");
    }

    void addEventCallback(InputEventCallback callback) {
        _callbacks.push_back(callback);
    }

    void update() {
        unsigned long now = millis();

        // Update level switch
        int levelReading = _hardware.readLevelSwitch();
        if (levelReading != _states.lastLevelRead) {
            _states.levelSwitchStableSince = now;
            _states.lastLevelRead = levelReading;
        } else if ((now - _states.levelSwitchStableSince) >= TimingConfig::DEBOUNCE_STABLE_MS) {
            if (levelReading != _states.levelSwitch) {
                _states.levelSwitch = levelReading;
                if (levelReading == LOW) {
                    _logger.info("Level switch stable: LOW (milk needed)");
                    notifyEvent(SystemEvent::LEVEL_SWITCH_LOW);
                } else {
                    _logger.info("Level switch stable: HIGH (milk sufficient)");
                    notifyEvent(SystemEvent::LEVEL_SWITCH_HIGH);
                }
            }
        }

        // Update wash standby
        int standbyReading = _hardware.readWashStandby();
        if (standbyReading != _states.lastWashStandbyRead) {
            _states.washStandbyStableSince = now;
            _states.lastWashStandbyRead = standbyReading;
        } else if ((now - _states.washStandbyStableSince) >= TimingConfig::DEBOUNCE_STABLE_MS) {
            if (standbyReading != _states.washStandby) {
                _states.washStandby = standbyReading;
                if (standbyReading == LOW) {
                    _logger.info("Wash standby switch stable: LOW (wash mode active)");
                    notifyEvent(SystemEvent::WASH_STANDBY_ACTIVATED);
                } else {
                    _logger.info("Wash standby switch stable: HIGH (normal mode)");
                    notifyEvent(SystemEvent::WASH_STANDBY_DEACTIVATED);
                }
            }
        }

        // Update wash dispense
        int dispenseReading = _hardware.readWashDispense();
        if (dispenseReading != _states.lastWashDispenseRead) {
            _states.washDispenseStableSince = now;
            _states.lastWashDispenseRead = dispenseReading;
        } else if ((now - _states.washDispenseStableSince) >= TimingConfig::DEBOUNCE_STABLE_MS) {
            if (dispenseReading != _states.washDispense) {
                _states.washDispense = dispenseReading;
                if (dispenseReading == LOW) {
                    _logger.info("Wash dispense switch stable: LOW (dispensing water)");
                    notifyEvent(SystemEvent::WASH_DISPENSE_ACTIVATED);
                } else {
                    _logger.info("Wash dispense switch stable: HIGH (water off)");
                    notifyEvent(SystemEvent::WASH_DISPENSE_DEACTIVATED);
                }
            }
        }
    }

    // State access
    const InputStates& getStates() const {
        return _states;
    }

    // LogObserver interface
    void onLogMessage(const String& message) override {
        // Could implement log filtering or additional processing here
    }

private:
    void notifyEvent(SystemEvent event) {
        for (auto& callback : _callbacks) {
            if (callback) {
                callback(event);
            }
        }
    }
};

#endif // INPUT_MANAGER_H