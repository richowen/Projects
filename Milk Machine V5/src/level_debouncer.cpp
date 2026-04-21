#include "level_debouncer.h"

LevelDebouncer::LevelDebouncer(uint32_t stableMsArg)
  : stableMs(stableMsArg),
    last_(false), stable_(false), stableSince_(0), initialized_(false) {}

void LevelDebouncer::reset(bool initialLevel, uint32_t nowMs) {
    last_        = initialLevel;
    stable_      = initialLevel;
    stableSince_ = nowMs;
    initialized_ = true;
}

bool LevelDebouncer::update(bool rawLevel, uint32_t nowMs) {
    if (!initialized_) { reset(rawLevel, nowMs); return false; }

    if (rawLevel != last_) {
        stableSince_ = nowMs;
        last_        = rawLevel;
        return false;
    }
    // Same as last raw — check if we've held long enough to commit a change.
    if ((uint32_t)(nowMs - stableSince_) >= stableMs && rawLevel != stable_) {
        stable_ = rawLevel;
        return true;
    }
    return false;
}
