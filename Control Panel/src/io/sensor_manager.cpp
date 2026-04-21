#include "sensor_manager.h"
#include "config.h"

// ========================================
// CONSTRUCTOR
// ========================================

SensorManager::SensorManager(IConfigManager* config, ILogger* logger)
    : _config(config), _logger(logger), _potPin(0),
      _currentMode(POT_MODE_BRIGHTNESS), _acBypassActive(false),
      _currentSetpoint(20), _lastSentSetpoint(-1), _temperatureChanged(false),
      _currentBrightness(50), _lastSentBrightness(-1), _brightnessChanged(false),
      _potReadIndex(0), _potTotal(0), _stableValue(-1), _stableTime(0) {
}

// ========================================
// ISENSORMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool SensorManager::begin() {
    _logger->info("Sensor Manager: Initializing...");

    _potPin = _config->getPin("pot_ac_temp");
    if (_potPin <= 0) {
        _logger->error("Sensor Manager: Invalid potentiometer pin configuration");
        return false;
    }

    pinMode(_potPin, INPUT);

    initializeAveraging();

    // Initialize with current reading - start in brightness mode
    int initialADC = readAveragedADC();
    _currentBrightness = adcToBrightness(initialADC);
    _currentSetpoint = adcToTemperature(initialADC);
    _stableValue = _currentBrightness;  // Start in brightness mode
    _stableTime = millis();  // Initialize stability timer

    _logger->logf("INFO", "Sensor Manager: Initialized in BRIGHTNESS mode with %d%% (ADC: %d)",
                  _currentBrightness, initialADC);

    // Ensure we don't immediately trigger a change on startup
    _lastSentBrightness = _currentBrightness;
    _lastSentSetpoint = _currentSetpoint;

    return true;
}

void SensorManager::update() {
    // Read new value and add to rolling average
    int newReading = analogRead(_potPin);
    _potTotal -= _potReadings[_potReadIndex];
    _potReadings[_potReadIndex] = newReading;
    _potTotal += _potReadings[_potReadIndex];
    _potReadIndex = (_potReadIndex + 1) % POT_SAMPLES;

    // Calculate averaged value
    int averagedADC = _potTotal / POT_SAMPLES;
    
    if (_currentMode == POT_MODE_BRIGHTNESS) {
        updateBrightnessMode(averagedADC);
    } else {
        updateTemperatureMode(averagedADC);
    }
}

int SensorManager::getTemperatureSetpoint() const {
    return _currentSetpoint;
}

bool SensorManager::hasTemperatureChanged() {
    bool changed = _temperatureChanged;
    _temperatureChanged = false;  // Clear flag after reading
    return changed;
}

int SensorManager::getRawADCReading() const {
    return _potTotal / POT_SAMPLES;
}

void SensorManager::setACBypassState(bool active) {
    if (_acBypassActive != active) {
        _acBypassActive = active;
        PotentiometerMode newMode = active ? POT_MODE_TEMPERATURE : POT_MODE_BRIGHTNESS;
        
        if (newMode != _currentMode) {
            _logger->logf("INFO", "Sensor Manager: Switching potentiometer mode to %s",
                         newMode == POT_MODE_BRIGHTNESS ? "BRIGHTNESS" : "TEMPERATURE");
            _currentMode = newMode;
            
            // Reset stability tracking when switching modes
            _stableValue = -1;
            _stableTime = millis();
        }
    }
}

bool SensorManager::isBrightnessMode() const {
    return _currentMode == POT_MODE_BRIGHTNESS;
}

int SensorManager::getBrightnessPercentage() const {
    return _currentBrightness;
}

bool SensorManager::hasBrightnessChanged() {
    bool changed = _brightnessChanged;
    _brightnessChanged = false;  // Clear flag after reading
    return changed;
}

// ========================================
// PRIVATE METHODS
// ========================================

void SensorManager::initializeAveraging() {
    _potTotal = 0;
    _potReadIndex = 0;

    // Fill averaging array with initial readings
    for (uint8_t i = 0; i < POT_SAMPLES; i++) {
        _potReadings[i] = analogRead(_potPin);
        _potTotal += _potReadings[i];
        delay(10);  // Small delay between readings
    }
}

int SensorManager::readAveragedADC() {
    return _potTotal / POT_SAMPLES;
}

int SensorManager::adcToTemperature(int adcValue) const {
    int minTemp, maxTemp;
    _config->getTemperatureRange(minTemp, maxTemp);

    // Map ADC range to temperature range (inverted for typical pot installation)
    return map(adcValue, 0, _config->getADCMaxValue(), maxTemp, minTemp);
}

int SensorManager::adcToBrightness(int adcValue) const {
    // Map ADC range to brightness percentage (0-100)
    // Inverted mapping: higher ADC = higher brightness (clockwise increases brightness)
    return map(adcValue, 0, _config->getADCMaxValue(), 100, 0);
}

void SensorManager::updateBrightnessMode(int averagedADC) {
    int brightnessValue = adcToBrightness(averagedADC);
    
    // Log if value changed (even if not stable)
    static int lastLoggedBrightness = -999;
    if (brightnessValue != lastLoggedBrightness) {
        _logger->logf("DEBUG", "Sensor Manager: Live brightness update: %d%% (ADC: %d, Stable: %s)",
                      brightnessValue, averagedADC, isValueStable(brightnessValue) ? "YES" : "NO");
        lastLoggedBrightness = brightnessValue;
    }
    
    // Update current brightness immediately for responsive feel
    _currentBrightness = brightnessValue;
    
    // Check stability for change detection
    if (isValueStable(brightnessValue)) {
        if (brightnessValue != _lastSentBrightness) {
            _brightnessChanged = true;
            _lastSentBrightness = brightnessValue;
            _logger->logf("INFO", "Sensor Manager: Brightness STABLE at %d%% - triggering change event", brightnessValue);
        }
    } else {
        updateStability(brightnessValue);
    }
}

void SensorManager::updateTemperatureMode(int averagedADC) {
    int temperatureValue = adcToTemperature(averagedADC);
    
    // Log if value changed (even if not stable)
    static int lastLoggedTemp = -999;
    if (temperatureValue != lastLoggedTemp) {
        _logger->logf("DEBUG", "Sensor Manager: Live temp update: %d°C (ADC: %d, Stable: %s)",
                      temperatureValue, averagedADC, isValueStable(temperatureValue) ? "YES" : "NO");
        lastLoggedTemp = temperatureValue;
    }
    
    // Update current setpoint immediately for responsive feel
    _currentSetpoint = temperatureValue;
    
    // Check stability for change detection
    if (isValueStable(temperatureValue)) {
        if (temperatureValue != _lastSentSetpoint) {
            _temperatureChanged = true;
            _lastSentSetpoint = temperatureValue;
            _logger->logf("INFO", "Sensor Manager: Temperature setpoint STABLE at %d°C - triggering change event", temperatureValue);
        }
    } else {
        updateStability(temperatureValue);
    }
}

bool SensorManager::isValueStable(int currentValue) {
    return (currentValue == _stableValue) &&
           ((millis() - _stableTime) >= _config->getPotStabilityDuration());
}

void SensorManager::updateStability(int value) {
    if (value != _stableValue) {
        _stableValue = value;
        _stableTime = millis();
    }
}