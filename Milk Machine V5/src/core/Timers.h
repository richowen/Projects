#pragma once
#include <Arduino.h>

namespace Timers {
    inline bool elapsed(unsigned long start, unsigned long interval) {
        unsigned long now = millis();
        if (now >= start) {
            return (now - start) >= interval;
        }
        // millis() overflowed
        return (now + (0xFFFFFFFFul - start) + 1ul) >= interval;
    }

    inline unsigned long now() {
        return millis();
    }

    struct Debounce {
        bool stableState{false};
        bool lastRaw{false};
        unsigned long tStart{0};
        unsigned long period{0};
        bool initialized{false};

        void begin(bool initialRaw, unsigned long debounceMs) {
            period = debounceMs;
            lastRaw = initialRaw;
            stableState = initialRaw;
            tStart = millis();
            initialized = true;
        }

        // Returns true if stable state changed after debounce
        bool update(bool raw) {
            if (!initialized) begin(raw, period == 0 ? 1000 : period);
            bool changed = false;
            if (raw != lastRaw) {
                lastRaw = raw;
                tStart = millis();
            } else if (stableState != raw && elapsed(tStart, period)) {
                stableState = raw;
                changed = true;
            }
            return changed;
        }
    };
}