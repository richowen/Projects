#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class DataStore {
public:
    void begin() {
        prefs.begin(AppCfg::PREFS_NS, false);
        totalMixes = prefs.getUInt("totalMixes", 0);
        errorCount = prefs.getUInt("errorCount", 0);
        lastMixTime = prefs.getULong("lastMixTime", 0);
        bootTime = millis();
        sessionMixes = 0;
    }

    void onMixCompleted() {
        sessionMixes++;
        totalMixes++;
        lastMixTime = millis();
        prefs.putUInt("totalMixes", totalMixes);
        prefs.putULong("lastMixTime", lastMixTime);
    }

    void onError() {
        errorCount++;
        prefs.putUInt("errorCount", errorCount);
    }

    // Metrics accessors
    uint32_t getTotalMixes() const { return totalMixes; }
    uint32_t getSessionMixes() const { return sessionMixes; }
    uint32_t getErrorCount() const { return errorCount; }
    uint32_t getUptimeHours() const {
        unsigned long uptimeSeconds = (millis() - bootTime) / 1000UL;
        return uptimeSeconds / 3600UL;
    }
    uint32_t getLastMixTime() const { return lastMixTime; }
    unsigned long getBootTimeMs() const { return bootTime; }

private:
    Preferences prefs;
    uint32_t sessionMixes{0};
    uint32_t totalMixes{0};
    uint32_t errorCount{0};
    uint32_t lastMixTime{0};
    unsigned long bootTime{0};
};