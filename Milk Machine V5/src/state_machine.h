#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "config.h"
#include "hardware_controller.h"
#include "logger.h"
#include <functional>

// State transition callback
using StateChangeCallback = std::function<void(SystemState oldState, SystemState newState)>;

class StateMachine {
private:
    Logger& _logger;
    HardwareController& _hardware;
    SystemState _currentState = SystemState::IDLE;
    unsigned long _stateChangeTime = 0;
    unsigned long _lastPeriodicMix = 0;
    std::vector<StateChangeCallback> _callbacks;

public:
    StateMachine(Logger& logger, HardwareController& hardware)
        : _logger(logger), _hardware(hardware) {}

    void initialize() {
        _logger.info("Initializing state machine...");
        _lastPeriodicMix = millis();
        _stateChangeTime = millis();
        _logger.info("State machine initialized.");
    }

    void addStateChangeCallback(StateChangeCallback callback) {
        _callbacks.push_back(callback);
    }

    void processEvent(SystemEvent event) {
        SystemState newState = _currentState;

        switch (_currentState) {
            case SystemState::IDLE:
                if (event == SystemEvent::WASH_STANDBY_ACTIVATED) {
                    newState = SystemState::WASH_STANDBY;
                    _hardware.allRelaysOff();
                } else if (event == SystemEvent::LEVEL_SWITCH_LOW) {
                    newState = SystemState::MIXING;
                    _hardware.mixingRelaysOn();
                } else if (event == SystemEvent::PERIODIC_MIX_TIMEOUT) {
                    newState = SystemState::PERIODIC_MIX;
                    _hardware.mixerOnlyOn();
                    _lastPeriodicMix = millis();
                }
                break;

            case SystemState::MIXING:
                if (event == SystemEvent::LEVEL_SWITCH_HIGH) {
                    newState = SystemState::POST_MIX;
                    _hardware.mixerOnlyOn();
                } else if (event == SystemEvent::MIXING_TIMEOUT) {
                    newState = SystemState::FAULT;
                    _hardware.allRelaysOff();
                }
                break;

            case SystemState::POST_MIX:
                if (event == SystemEvent::POST_MIX_TIMEOUT) {
                    newState = SystemState::IDLE;
                    _hardware.allRelaysOff();
                }
                break;

            case SystemState::PERIODIC_MIX:
                if (event == SystemEvent::PERIODIC_MIX_TIMEOUT) {
                    newState = SystemState::IDLE;
                    _hardware.allRelaysOff();
                } else if (event == SystemEvent::LEVEL_SWITCH_LOW) {
                    newState = SystemState::MIXING;
                    _hardware.mixingRelaysOn();
                }
                break;

            case SystemState::WASH_STANDBY:
                if (event == SystemEvent::WASH_STANDBY_DEACTIVATED) {
                    newState = SystemState::IDLE;
                    _hardware.allRelaysOff();
                    _lastPeriodicMix = millis(); // Reset periodic timer
                } else if (event == SystemEvent::WASH_DISPENSE_ACTIVATED) {
                    _hardware.waterOnlyOn();
                } else if (event == SystemEvent::WASH_DISPENSE_DEACTIVATED) {
                    _hardware.allRelaysOff();
                }
                break;

            case SystemState::FAULT:
                if (event == SystemEvent::FAULT_DETECTED) {
                    // Stay in fault, but log
                    _logger.error("Additional fault detected while in FAULT state");
                }
                break;
        }

        if (newState != _currentState) {
            transitionTo(newState);
        }
    }

    void update() {
        unsigned long now = millis();

        // Check for timeouts
        switch (_currentState) {
            case SystemState::MIXING:
                if ((now - _stateChangeTime) >= SafetyConfig::MAX_CONTINUOUS_MIX_MS) {
                    processEvent(SystemEvent::MIXING_TIMEOUT);
                }
                break;

            case SystemState::POST_MIX:
                if ((now - _stateChangeTime) >= TimingConfig::POST_MIX_DURATION_MS) {
                    processEvent(SystemEvent::POST_MIX_TIMEOUT);
                }
                break;

            case SystemState::PERIODIC_MIX:
                if ((now - _stateChangeTime) >= TimingConfig::PERIODIC_MIX_DURATION_MS) {
                    processEvent(SystemEvent::PERIODIC_MIX_TIMEOUT);
                }
                break;

            case SystemState::IDLE:
                if ((now - _lastPeriodicMix) >= TimingConfig::PERIODIC_MIX_INTERVAL_MS) {
                    processEvent(SystemEvent::PERIODIC_MIX_TIMEOUT);
                }
                break;

            default:
                break;
        }
    }

    SystemState getCurrentState() const {
        return _currentState;
    }

    unsigned long getStateTime() const {
        return millis() - _stateChangeTime;
    }

private:
    void transitionTo(SystemState newState) {
        SystemState oldState = _currentState;
        _currentState = newState;
        _stateChangeTime = millis();

        // Log transition
        String transitionMsg = "STATE: " + stateToString(oldState) + " -> " + stateToString(newState);
        _logger.info(transitionMsg);

        // Notify callbacks
        for (auto& callback : _callbacks) {
            if (callback) {
                callback(oldState, newState);
            }
        }
    }

    String stateToString(SystemState state) const {
        switch (state) {
            case SystemState::IDLE: return "IDLE";
            case SystemState::MIXING: return "MIXING";
            case SystemState::POST_MIX: return "POST_MIX";
            case SystemState::PERIODIC_MIX: return "PERIODIC_MIX";
            case SystemState::WASH_STANDBY: return "WASH_STANDBY";
            case SystemState::FAULT: return "FAULT";
            default: return "UNKNOWN";
        }
    }
};

#endif // STATE_MACHINE_H