#ifndef HA_CLIENT_H
#define HA_CLIENT_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "interfaces.h"

/**
 * @brief Home Assistant API client
 *
 * Handles communication with Home Assistant REST API including
 * authentication, command sending, and error handling.
 */
class HAClient : public IHAClient {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager reference
     * @param wifiManager WiFi manager reference
     * @param logger Logger reference
     */
    HAClient(IConfigManager* config, IWiFiManager* wifiManager, ILogger* logger);

    // IHAClient interface implementation
    bool begin(const char* url, const char* token) override;
    bool sendCommand(const char* entityId, const char* service, JsonDocument* data = nullptr) override;
    bool isConnected() const override;
    String getLastError() const override;

private:
    IConfigManager* _config;
    IWiFiManager* _wifiManager;
    ILogger* _logger;

    String _baseUrl;
    String _authToken;
    String _lastError;

    /**
     * @brief Determine domain from entity ID
     * @param entityId Full entity ID (e.g., "switch.living_room_light")
     * @return domain string (e.g., "switch")
     */
    String extractDomain(const char* entityId) const;

    /**
     * @brief Build full API URL for service call
     * @param domain Entity domain
     * @param service Service name
     * @return complete URL string
     */
    String buildServiceUrl(const String& domain, const String& service) const;

    /**
     * @brief Create JSON payload for API call
     * @param entityId Target entity ID
     * @param data Optional additional data
     * @return JSON string payload
     */
    String createPayload(const char* entityId, JsonDocument* data) const;

    /**
     * @brief Send HTTP request and handle response
     * @param url API endpoint URL
     * @param payload JSON payload
     * @return true if successful (200-299 status code)
     */
    bool sendRequest(const String& url, const String& payload);

    /**
     * @brief Log command details for debugging
     * @param entityId Target entity
     * @param service Service called
     * @param payload JSON payload sent
     */
    void logCommand(const char* entityId, const char* service, const String& payload) const;
};

#endif // HA_CLIENT_H