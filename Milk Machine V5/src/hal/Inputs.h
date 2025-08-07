#pragma once
#include <Arduino.h>
#include "../core/Config.h"
#include "../core/Timers.h"

namespace HAL {

class Inputs {
public:
    void begin() {
        pinMode(Pins::LEVEL_SWITCH, INPUT_PULLUP);
        pinMode(Pins::WASH_STANDBY, INPUT_PULLUP);
        pinMode(Pins::WASH_DISPENSE, INPUT_PULLUP);

        // Initialize debouncers with current raw states (LOW = active)
        bool lvl = readLevelRaw();
        bool wst = readWashStandbyRaw();
        bool wdp = readWashDispenseRaw();
        levelDebounce.begin(lvl, Times::LEVEL_DEBOUNCE_TIME);
        washStandbyDebounce.begin(wst, Times::LEVEL_DEBOUNCE_TIME);
        washDispenseDebounce.begin(wdp, Times::LEVEL_DEBOUNCE_TIME);
    }

    // Poll inputs, return bitmask of which stable states changed
    // bit0: level, bit1: wash_standby, bit2: wash_dispense
    uint8_t poll() {
        uint8_t changed = 0;
        if (levelDebounce.update(readLevelRaw()))        changed |= 0x01;
        if (washStandbyDebounce.update(readWashStandbyRaw())) changed |= 0x02;
        if (washDispenseDebounce.update(readWashDispenseRaw())) changed |= 0x04;
        return changed;
    }

    // Stable states (true = active/pressed)
    bool levelActive() const        { return levelDebounce.stableState; }         // LOW means active
    bool washStandbyActive() const  { return washStandbyDebounce.stableState; }
    bool washDispenseActive() const { return washDispenseDebounce.stableState; }

private:
    // Raw reads (convert LOW to true)
    static bool readLevelRaw()        { return digitalRead(Pins::LEVEL_SWITCH) == LOW; }
    static bool readWashStandbyRaw()  { return digitalRead(Pins::WASH_STANDBY) == LOW; }
    static bool readWashDispenseRaw() { return digitalRead(Pins::WASH_DISPENSE) == LOW; }

    Timers::Debounce levelDebounce{};
    Timers::Debounce washStandbyDebounce{};
    Timers::Debounce washDispenseDebounce{};
};

} // namespace HAL