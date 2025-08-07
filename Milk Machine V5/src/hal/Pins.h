#pragma once
#include <Arduino.h>
#include "../core/Config.h"

namespace HAL {

    inline void initPins() {
        // Relays as outputs (active LOW), default OFF = HIGH
        pinMode(Pins::RELAY_AUGER, OUTPUT);
        pinMode(Pins::RELAY_AGITATOR, OUTPUT);
        pinMode(Pins::RELAY_MIXER, OUTPUT);
        pinMode(Pins::RELAY_WATER, OUTPUT);

        digitalWrite(Pins::RELAY_AUGER, HIGH);
        digitalWrite(Pins::RELAY_AGITATOR, HIGH);
        digitalWrite(Pins::RELAY_MIXER, HIGH);
        digitalWrite(Pins::RELAY_WATER, HIGH);

        // Inputs as pullups
        pinMode(Pins::LEVEL_SWITCH, INPUT_PULLUP);
        pinMode(Pins::WASH_STANDBY, INPUT_PULLUP);
        pinMode(Pins::WASH_DISPENSE, INPUT_PULLUP);
    }

    // Helpers for active-low relay semantics
    inline void relayOn(int pin)  { digitalWrite(pin, LOW); }
    inline void relayOff(int pin) { digitalWrite(pin, HIGH); }

} // namespace HAL