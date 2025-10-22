#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "interfaces.h"

/**
 * @brief Sensor manager for analog inputs (potentiometers, etc.)
 *
 * Handles potentiometer readings with averaging, debouncing, and temperature
 * setpoint management for AC control.
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

private:
    IConfigManager* _config;
    ILogger* _logger;

    int _potPin;
    int _currentSetpoint;
    int _lastSentSetpoint;
    bool _temperatureChanged;

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