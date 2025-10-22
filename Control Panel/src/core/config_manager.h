#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "interfaces.h"
#include "config.h"

/**
 * @brief Configuration manager for the control panel
 *
 * Centralizes all configuration access and provides runtime validation.
 * Wraps the existing config.h defines with runtime flexibility.
 */
class ConfigManager : public IConfigManager {
public:
    /**
     * @brief Constructor
     */
    ConfigManager();

    // IConfigManager interface implementation
    bool load() override;
    bool save() override;
    const char* getWiFiSSID() const override;
    const char* getWiFiPassword() const override;
    const char* getHAURL() const override;
    const char* getHAToken() const override;
    void getTemperatureRange(int& minTemp, int& maxTemp) const override;
    int getPin(const char* pinType) const override;
    unsigned long getTiming(const char* timingType) const override;
    const char* getEntityId(const char* entityType) const override;

    /**
     * @brief Check if debug mode is enabled
     * @return true if debug mode
     */
    bool isDebugMode() const;

    /**
     * @brief Get display intensity setting
     * @return intensity value (0-15)
     */
    uint8_t getDisplayIntensity() const;

    /**
     * @brief Get debounce delay for inputs
     * @return delay in milliseconds
     */
    unsigned long getDebounceDelay() const;

    /**
     * @brief Get ADC maximum value
     * @return ADC max value
     */
    int getADCMaxValue() const;

    /**
     * @brief Get WiFi connection timeout
     * @return timeout in milliseconds
     */
    unsigned long getWiFiTimeout() const;

    /**
     * @brief Get HTTP request timeout
     * @return timeout in milliseconds
     */
    unsigned long getHTTPTimeout() const;

    /**
     * @brief Get potentiometer sample count for averaging
     * @return sample count
     */
    uint8_t getPotSamples() const;

    /**
     * @brief Get potentiometer stability duration
     * @return stability time in milliseconds
     */
    unsigned long getPotStabilityDuration() const;

    /**
     * @brief Get PC shutdown hold time
     * @return hold time in milliseconds
     */
    unsigned long getPCShutdownHoldTime() const;

    /**
     * @brief Get Plex on hold time
     * @return hold time in milliseconds
     */
    unsigned long getPlexOnHoldTime() const;

    /**
     * @brief Get WiFi retry interval
     * @return retry interval in milliseconds
     */
    unsigned long getWiFiRetryInterval() const;

    /**
     * @brief Get display timeout
     * @return timeout in milliseconds
     */
    unsigned long getDisplayTimeout() const;

    /**
     * @brief Get display height
     * @return height in pixels
     */
    uint8_t getDisplayHeight() const;

private:
    // Configuration validation
    bool validateConfig() const;

    // Pin mapping
    int getDisplayCSPin() const;
    int getPotPin() const;
    int getButtonPin(uint8_t buttonIndex) const;
    int getStatusLEDPin() const;
};

#endif // CONFIG_MANAGER_H