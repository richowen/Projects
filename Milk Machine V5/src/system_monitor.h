#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "config.h"
#include "state_machine.h"
#include "logger.h"
#include <ESP.h>

class SystemMonitor {
private:
    Logger& _logger;
    SystemState _currentState = SystemState::IDLE;
    unsigned int _faultCount = 0;
    unsigned long _lastFaultTimestamp = 0;

public:
    SystemMonitor(Logger& logger) : _logger(logger) {}

    void update(SystemState state, unsigned long stateTimeMs = 0) {
        _currentState = state;

        // Check heap
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < SafetyConfig::MIN_SAFE_HEAP) {
            _logger.error("CRITICAL: Low heap " + String(freeHeap) + " bytes < " + String(SafetyConfig::MIN_SAFE_HEAP) + " threshold");
            triggerFault("Low heap");
            return;
        }

        // Sanity check for stuck mixing - ensure we don't get stuck beyond safety limits
        if (state == SystemState::MIXING && stateTimeMs > SafetyConfig::MAX_CONTINUOUS_MIX_MS + 10000UL) {
            _logger.error("Sanity check: MIXING state exceeded safety limit + 10s margin (" + String(stateTimeMs) + "ms)");
            triggerFault("Mixing stuck beyond safety margin");
            return;
        }
    }

    void triggerFault(const String& reason) {
        unsigned long now = millis();

        if ((now - _lastFaultTimestamp) <= SafetyConfig::FAULT_WINDOW_MS) {
            _faultCount++;
        } else {
            _faultCount = 1;
        }

        _lastFaultTimestamp = now;

        _logger.error("FAULT: " + reason + " (count: " + String(_faultCount) + ")");

        if (_faultCount >= SafetyConfig::MAX_FAULTS_BEFORE_REBOOT) {
            _logger.error("Too many faults - rebooting");
            delay(100);
            ESP.restart();
        }
    }

    unsigned int getFaultCount() const {
        return _faultCount;
    }
};

#endif // SYSTEM_MONITOR_H