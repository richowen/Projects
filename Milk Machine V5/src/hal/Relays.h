#pragma once
#include <Arduino.h>
#include "../core/Config.h"
#include "Pins.h"

namespace HAL {

class Relays {
public:
    void begin() {
        HAL::initPins(); // ensure pins set and relays off
    }

    // Individual controls
    inline void augerOn()    { HAL::relayOn(Pins::RELAY_AUGER); }
    inline void augerOff()   { HAL::relayOff(Pins::RELAY_AUGER); }

    inline void agitatorOn() { HAL::relayOn(Pins::RELAY_AGITATOR); }
    inline void agitatorOff(){ HAL::relayOff(Pins::RELAY_AGITATOR); }

    inline void mixerOn()    { HAL::relayOn(Pins::RELAY_MIXER); }
    inline void mixerOff()   { HAL::relayOff(Pins::RELAY_MIXER); }

    inline void waterOn()    { HAL::relayOn(Pins::RELAY_WATER); }
    inline void waterOff()   { HAL::relayOff(Pins::RELAY_WATER); }

    // Grouped ops
    inline void allOff() {
        augerOff();
        agitatorOff();
        mixerOff();
        waterOff();
    }

    inline void allOn() {
        augerOn();
        agitatorOn();
        mixerOn();
        waterOn();
    }
};

} // namespace HAL