#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "interfaces.h"

/**
 * @brief WiFi connectivity manager
 *
 * Handles WiFi connection, reconnection, and status monitoring.
 * Provides clean interface for network connectivity management.
 */
class WiFiManager : public IWiFiManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager reference
     * @param logger Logger reference
     */
    WiFiManager(IConfigManager* config, ILogger* logger);

    // IWiFiManager interface implementation
    bool begin() override;
    bool isConnected() const override;
    uint8_t getSignalStrength() const override;
    String getIPAddress() const override;
    bool reconnect() override;
    void update() override;

private:
    IConfigManager* _config;
    ILogger* _logger;

    unsigned long _lastReconnectAttempt;
    bool _wasConnected;

    /**
     * @brief Attempt to connect to WiFi
     * @return true if connected successfully
     */
    bool connect();

    /**
     * @brief Convert RSSI to signal strength level (0-3)
     * @param rssi RSSI value
     * @return signal level
     */
    uint8_t rssiToSignalLevel(int rssi) const;

    /**
     * @brief Check if reconnection should be attempted
     * @return true if should attempt reconnection
     */
    bool shouldAttemptReconnect() const;
};

#endif // WIFI_MANAGER_H