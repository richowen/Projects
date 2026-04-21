#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "interfaces.h"

/**
 * @brief Potentiometer operation mode
 */
enum PotentiometerMode {
    POT_MODE_BRIGHTNESS,    // Default: control light brightness
    POT_MODE_TEMPERATURE    // AC bypass active: control AC temperature
};

/**
 * @brief Sensor manager for analog inputs (potentiometers, etc.)
 *
 * Handles potentiometer readings with averaging, debouncing, and dual-mode
 * operation for brightness control (default) and temperature setpoint (AC bypass active).
 */
class SensorManager : public ISensorManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager reference
     * @param logger Logger reference
     */
    SensorManager(IConfigManager* config, ILogger* logger);

    // ISensorManager interface implementation
    bool begin() override;
    void update() override;
    int getTemperatureSetpoint() const override;
    bool hasTemperatureChanged() override;
    int getRawADCReading() const override;
    void setACBypassState(bool active) override;
    bool isBrightnessMode() const override;
    int getBrightnessPercentage() const override;
    bool hasBrightnessChanged() override;

private:
    IConfigManager* _config;
    ILogger* _logger;

    int _potPin;
    
    // Mode tracking
    PotentiometerMode _currentMode;
    bool _acBypassActive;
    
    // Temperature mode
    int _currentSetpoint;
    int _lastSentSetpoint;
    bool _temperatureChanged;
    
    // Brightness mode
    int _currentBrightness;
    int _lastSentBrightness;
    bool _brightnessChanged;

    // Potentiometer averaging
    static const uint8_t POT_SAMPLES = 10;
    int _potReadings[POT_SAMPLES];
    uint8_t _potReadIndex;
    int _potTotal;

    // Stability tracking
    int _stableValue;
    unsigned long _stableTime;

    /**
     * @brief Initialize potentiometer averaging array
     */
    void initializeAveraging();

    /**
     * @brief Read and average potentiometer value
     * @return averaged ADC reading
     */
    int readAveragedADC();

    /**
     * @brief Convert ADC reading to temperature setpoint
     * @param adcValue Raw ADC value
     * @return temperature in °C
     */
    int adcToTemperature(int adcValue) const;

    /**
     * @brief Convert ADC reading to brightness percentage
     * @param adcValue Raw ADC value
     * @return brightness percentage (0-100)
     */
    int adcToBrightness(int adcValue) const;

    /**
     * @brief Update potentiometer in brightness mode
     * @param averagedADC Averaged ADC reading
     */
    void updateBrightnessMode(int averagedADC);

    /**
     * @brief Update potentiometer in temperature mode
     * @param averagedADC Averaged ADC reading
     */
    void updateTemperatureMode(int averagedADC);

    /**
     * @brief Check if potentiometer value is stable
     * @param currentValue Current averaged value
     * @return true if stable
     */
    bool isValueStable(int currentValue);

    /**
     * @brief Update stability tracking
     * @param value Current value
     */
    void updateStability(int value);
};

#endif // SENSOR_MANAGER_H