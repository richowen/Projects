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

    bool begin() {
        // Configure ADC
        analogReadResolution(12);  // 12-bit resolution (0-4095)
        analogSetAttenuation(ADC_11db);  // Set to 11dB attenuation for full 0-3.3V range
        
        // Initialize baseline values
        _augerBaseline = 0;
        _mixerBaseline = 0;
        _calibrated = false;

        // Verify pins are valid ADC pins
        if (!_verifyPins()) {
            return false;
        }

        return true;
    }

    // Calibrate baseline current when motors are off
    bool calibrateBaseline() {
        _augerMedian.clear();
        _mixerMedian.clear();

        // Take 100 samples to establish baseline
        for(int i = 0; i < 100; i++) {
            int augerReading = analogRead(_augerCurrentPin);
            int mixerReading = analogRead(_mixerCurrentPin);
            
            // Verify readings are valid
            if (augerReading < 0 || augerReading > 4095 || 
                mixerReading < 0 || mixerReading > 4095) {
                return false;
            }
            
            _augerMedian.add(augerReading);
            _mixerMedian.add(mixerReading);
            delay(10);
        }

        _augerBaseline = _augerMedian.getMedian();
        _mixerBaseline = _mixerMedian.getMedian();
        
        // Verify baselines are reasonable
        if (_augerBaseline < 0 || _augerBaseline > 4095 ||
            _mixerBaseline < 0 || _mixerBaseline > 4095) {
            return false;
        }

        _calibrated = true;

        Serial.print("Baseline values - Auger: ");
        Serial.print(_augerBaseline);
        Serial.print(" Mixer: ");
        Serial.println(_mixerBaseline);

        return true;
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

    bool _verifyPins() {
        // ESP32 ADC1 pins: 32-39
        // ESP32 ADC2 pins: 0,2,4,12-15,25-27
        if ((_augerCurrentPin >= 32 && _augerCurrentPin <= 39) ||
            (_mixerCurrentPin >= 32 && _mixerCurrentPin <= 39)) {
            return true;
        }
        return false;
    }

    // Convert ADC reading to current in amps using ACS712
    float getCurrentDraw(int reading, int baseline) {
        // Convert ADC reading to voltage
        float voltage = (reading - baseline) * (VCC / ADC_RESOLUTION);
        
        // Convert voltage to current based on ACS712 sensitivity
        return voltage / (MV_PER_AMP / 1000.0);
    }
};

#endif // MOTOR_MONITOR_H
