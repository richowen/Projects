#ifndef HARDWARE_CONTROLLER_H
#define HARDWARE_CONTROLLER_H

#include "config.h"
#include "logger.h"
#include <Arduino.h>

// Relay state tracking
struct RelayStates {
    bool auger = false;
    bool agitator = false;
    bool mixer = false;
    bool water = false;
};

class HardwareController {
private:
    Logger& _logger;
    RelayStates _relayStates;

public:
    HardwareController(Logger& logger) : _logger(logger) {}

    void initialize() {
        _logger.info("Initializing hardware...");

        // Set relay pins to HIGH before OUTPUT (active LOW relays)
        presetOutputHigh(HardwarePins::RELAY_AUGER);
        presetOutputHigh(HardwarePins::RELAY_AGITATOR);
        presetOutputHigh(HardwarePins::RELAY_MIXER);
        presetOutputHigh(HardwarePins::RELAY_WATER);

        // Input pins with pull-up
        pinMode(HardwarePins::LEVEL_SWITCH, INPUT_PULLUP);
        pinMode(HardwarePins::WASH_STANDBY_PIN, INPUT_PULLUP);
        pinMode(HardwarePins::WASH_DISPENSE_PIN, INPUT_PULLUP);

        // Ensure safe state
        allRelaysOff();
        _logger.info("Hardware initialized. All relays OFF.");
    }

    // Relay control methods
    void applyRelayState(bool auger, bool agitator, bool mixer, bool water) {
        if (auger != _relayStates.auger) {
            digitalWrite(HardwarePins::RELAY_AUGER, auger ? LOW : HIGH);
            _relayStates.auger = auger;
        }
        if (agitator != _relayStates.agitator) {
            digitalWrite(HardwarePins::RELAY_AGITATOR, agitator ? LOW : HIGH);
            _relayStates.agitator = agitator;
        }
        if (mixer != _relayStates.mixer) {
            digitalWrite(HardwarePins::RELAY_MIXER, mixer ? LOW : HIGH);
            _relayStates.mixer = mixer;
        }
        if (water != _relayStates.water) {
            digitalWrite(HardwarePins::RELAY_WATER, water ? LOW : HIGH);
            _relayStates.water = water;
        }
    }

    void allRelaysOff() {
        applyRelayState(false, false, false, false);
    }

    void mixingRelaysOn() {
        applyRelayState(true, true, true, true);
    }

    void mixerOnlyOn() {
        applyRelayState(false, false, true, false);
    }

    void waterOnlyOn() {
        applyRelayState(false, false, false, true);
    }

    // Input reading methods
    int readLevelSwitch() {
        return digitalRead(HardwarePins::LEVEL_SWITCH);
    }

    int readWashStandby() {
        return digitalRead(HardwarePins::WASH_STANDBY_PIN);
    }

    int readWashDispense() {
        return digitalRead(HardwarePins::WASH_DISPENSE_PIN);
    }

    // State access
    const RelayStates& getRelayStates() const {
        return _relayStates;
    }

private:
    void presetOutputHigh(int pin) {
        digitalWrite(pin, HIGH);
        pinMode(pin, OUTPUT);
    }
};

#endif // HARDWARE_CONTROLLER_H