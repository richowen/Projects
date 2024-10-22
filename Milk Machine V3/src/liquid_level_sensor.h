// liquid_level_sensor.h
#ifndef LIQUID_LEVEL_SENSOR_H
#define LIQUID_LEVEL_SENSOR_H

#include <Arduino.h>

class DualProbeSensor {
public:
    // Constructor takes the two probe pins
    DualProbeSensor(int probe1Pin, int probe2Pin) : 
        _probe1Pin(probe1Pin),
        _probe2Pin(probe2Pin) {}

    void begin() {
        // Configure pins with internal pullup resistors
        // When liquid connects probe to ground, we'll read LOW
        pinMode(_probe1Pin, INPUT_PULLUP);
        pinMode(_probe2Pin, INPUT_PULLUP);
    }

    void update() {
        unsigned long currentTime = millis();
        
        // Only update if debounce period has passed
        if (currentTime - _lastDebounceTime >= DEBOUNCE_DELAY) {
            // Read current states (LOW means liquid is present)
            bool probe1State = (digitalRead(_probe1Pin) == LOW);
            bool probe2State = (digitalRead(_probe2Pin) == LOW);
            
            // Update states if they've been stable
            if (probe1State == _lastProbe1State && probe2State == _lastProbe2State) {
                _probe1Active = probe1State;
                _probe2Active = probe2State;
            }
            
            _lastProbe1State = probe1State;
            _lastProbe2State = probe2State;
            _lastDebounceTime = currentTime;
        }
    }

    // Returns true when no probes are connected (start mixing condition)
    bool shouldStartMixing() const {
        return !_probe1Active && !_probe2Active;
    }

    // Returns true when both probes are connected (stop mixing condition)
    bool shouldStopMixing() const {
        return _probe1Active && _probe2Active;
    }

    // Individual probe states for debugging
    bool isProbe1Connected() const { return _probe1Active; }
    bool isProbe2Connected() const { return _probe2Active; }

private:
    const int _probe1Pin;
    const int _probe2Pin;
    static const unsigned long DEBOUNCE_DELAY = 1000; // 50ms debounce

    bool _probe1Active = false;
    bool _probe2Active = false;
    bool _lastProbe1State = false;
    bool _lastProbe2State = false;
    unsigned long _lastDebounceTime = 0;
};

#endif // LIQUID_LEVEL_SENSOR_H