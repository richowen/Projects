// level_debouncer.h — hardware-free debounce FSM.
#ifndef LEVEL_DEBOUNCER_H
#define LEVEL_DEBOUNCER_H

#include <stdint.h>

class LevelDebouncer {
public:
    // stableMs = debounce window. initialLevel = level read at init time.
    LevelDebouncer(uint32_t stableMs = 50);

    void reset(bool initialLevel, uint32_t nowMs);

    // Feed a raw sample. Returns true when the stable state changes.
    bool update(bool rawLevel, uint32_t nowMs);

    bool stable() const { return stable_; }

    // Public so callers can reconfigure at runtime if desired.
    uint32_t stableMs;

private:
    bool     last_;       // last raw sample
    bool     stable_;     // debounced output
    uint32_t stableSince_;
    bool     initialized_;
};

#endif
