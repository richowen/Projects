// motor_monitor.h
#ifndef MOTOR_MONITOR_H
#define MOTOR_MONITOR_H

#include <Arduino.h>
#include <RunningMedian.h>
#include "config.h"

class MotorMonitor {
public:
    MotorMonitor(uint8_t augerCurrentPin, uint8_t mixerCurrentPin) 
        : _augerCurrentPin(augerCurrentPin)
        , _mixerCurrentPin(mixerCurrentPin)
        , _augerMedian(15)  // Keep 15 samples for median filtering
        , _mixerMedian(15)
    {
    }

    void begin() {
        // Configure ADC
        analogReadResolution(12);  // 12-bit resolution (0-4095)
        analogSetAttenuation(ADC_11db);  // Set to 11dB attenuation for full 0-3.3V range
        
        // Initialize baseline values
        _augerBaseline = 0;
        _mixerBaseline = 0;
        _calibrated = false;
    }

    // Calibrate baseline current when motors are off
    void calibrateBaseline() {
        _augerMedian.clear();
        _mixerMedian.clear();

        // Take 100 samples to establish baseline
        for(int i = 0; i < 100; i++) {
            _augerMedian.add(analogRead(_augerCurrentPin));
            _mixerMedian.add(analogRead(_mixerCurrentPin));
            delay(10);
        }

        _augerBaseline = _augerMedian.getMedian();
        _mixerBaseline = _mixerMedian.getMedian();
        _calibrated = true;

        Serial.print("Baseline values - Auger: ");
        Serial.print(_augerBaseline);
        Serial.print(" Mixer: ");
        Serial.println(_mixerBaseline);
    }

    // Update current readings
    void update() {
        if (!_calibrated) return;

        _augerMedian.add(analogRead(_augerCurrentPin));
        _mixerMedian.add(analogRead(_mixerCurrentPin));
    }

    // Check if auger current indicates empty hopper
    bool isHopperEmpty() {
        if (!_calibrated) return false;

        float augerCurrent = getCurrentDraw(_augerMedian.getMedian(), _augerBaseline);
        
        // Only check if current is consistently low for multiple readings
        if (augerCurrent < EMPTY_HOPPER_THRESHOLD) {
            _lowCurrentCount++;
            if (_lowCurrentCount >= EMPTY_HOPPER_COUNT) {
                return true;
            }
        } else {
            _lowCurrentCount = 0;
        }
        return false;
    }

    // Check if mixer current indicates jam or failure
    bool isMixerOverload() {
        if (!_calibrated) return false;

        float mixerCurrent = getCurrentDraw(_mixerMedian.getMedian(), _mixerBaseline);
        
        // Only trigger on sustained high current
        if (mixerCurrent > MIXER_OVERLOAD_THRESHOLD) {
            _highCurrentCount++;
            if (_highCurrentCount >= OVERLOAD_COUNT) {
                return true;
            }
        } else {
            _highCurrentCount = 0;
        }
        return false;
    }

    // Get current readings for diagnostics
    float getAugerCurrent() {
        if (!_calibrated) return 0.0;
        return getCurrentDraw(_augerMedian.getMedian(), _augerBaseline);
    }

    float getMixerCurrent() {
        if (!_calibrated) return 0.0;
        return getCurrentDraw(_mixerMedian.getMedian(), _mixerBaseline);
    }

private:
    const uint8_t _augerCurrentPin;
    const uint8_t _mixerCurrentPin;
    RunningMedian _augerMedian;
    RunningMedian _mixerMedian;
    int _augerBaseline;
    int _mixerBaseline;
    bool _calibrated;
    uint8_t _lowCurrentCount = 0;
    uint8_t _highCurrentCount = 0;

    // Convert ADC reading to current in amps using ACS712
    float getCurrentDraw(int reading, int baseline) {
        // Convert ADC reading to voltage
        float voltage = (reading - baseline) * (VCC / ADC_RESOLUTION);
        
        // Convert voltage to current based on ACS712 sensitivity
        return voltage / (MV_PER_AMP / 1000.0);
    }
};

#endif // MOTOR_MONITOR_H
