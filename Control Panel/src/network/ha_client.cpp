#include "ha_client.h"

// ========================================
// CONSTRUCTOR
// ========================================

HAClient::HAClient(IConfigManager* config, IWiFiManager* wifiManager, ILogger* logger)
    : _config(config), _wifiManager(wifiManager), _logger(logger) {
}

// ========================================
// IHACLIENT INTERFACE IMPLEMENTATION
// ========================================

bool HAClient::begin(const char* url, const char* token) {
    _baseUrl = String(url);
    _authToken = String(token);

    _logger->logf("INFO", "HA Client: Initialized with URL: %s", url);

    // Basic validation
    if (_baseUrl.length() == 0 || _authToken.length() == 0) {
        _lastError = "Invalid URL or token";
        _logger->error("HA Client: Invalid configuration");
        return false;
    }

    return true;
}

bool HAClient::sendCommand(const char* entityId, const char* service, JsonDocument* data) {
    if (_config->isDebugMode()) {
        // Debug mode: simulate command
        _logger->logf("INFO", "HA Client: [DEBUG] Would send command to %s service: %s", entityId, service);

        String payload = createPayload(entityId, data);
        logCommand(entityId, service, payload);

        _lastError = "";
        return true;
    }

    // Check WiFi connectivity
    if (!_wifiManager->isConnected()) {
        _lastError = "WiFi not connected";
        _logger->error("HA Client: Cannot send command - WiFi not connected");
        return false;
    }

    // Extract domain and build URL
    String domain = extractDomain(entityId);
    if (domain.length() == 0) {
        _lastError = "Invalid entity ID format";
        _logger->error("HA Client: Invalid entity ID format");
        return false;
    }

    String url = buildServiceUrl(domain, service);
    String payload = createPayload(entityId, data);

    logCommand(entityId, service, payload);

    return sendRequest(url, payload);
}

bool HAClient::isConnected() const {
    // For now, assume connected if WiFi is connected
    // Future enhancement: ping HA API
    return _wifiManager->isConnected();
}

String HAClient::getLastError() const {
    return _lastError;
}

// ========================================
// PRIVATE METHODS
// ========================================

String HAClient::extractDomain(const char* entityId) const {
    String entityStr = String(entityId);
    int dotIndex = entityStr.indexOf('.');

    if (dotIndex == -1) {
        return "";
    }

    return entityStr.substring(0, dotIndex);
}

String HAClient::buildServiceUrl(const String& domain, const String& service) const {
    return _baseUrl + "/api/services/" + domain + "/" + service;
}

String HAClient::createPayload(const char* entityId, JsonDocument* data) const {
    StaticJsonDocument<256> payload;
    payload["entity_id"] = entityId;

    if (data != nullptr) {
        JsonObject dataObj = data->as<JsonObject>();
        for (JsonPair kv : dataObj) {
            payload[kv.key()] = kv.value();
        }
    }

    String jsonString;
    serializeJson(payload, jsonString);
    return jsonString;
}

bool HAClient::sendRequest(const String& url, const String& payload) {
    HTTPClient http;

    _logger->logf("DEBUG", "HA Client: Sending request to %s", url.c_str());

    http.begin(url);
    http.setTimeout(_config->getTiming("http_timeout"));
    http.addHeader("Authorization", "Bearer " + _authToken);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(payload);

    bool success = (httpCode >= 200 && httpCode < 300);

    if (success) {
        _logger->logf("INFO", "HA Client: Command sent successfully (HTTP %d)", httpCode);
        _lastError = "";
    } else {
        _lastError = "HTTP " + String(httpCode) + ": " + http.errorToString(httpCode);
        _logger->logf("ERROR", "HA Client: Command failed - %s", _lastError.c_str());
    }

    // Log response body if available
    String response = http.getString();
    if (response.length() > 0) {
        _logger->logf("DEBUG", "HA Client: Response: %s", response.c_str());
    }

    http.end();
    return success;
}

void HAClient::logCommand(const char* entityId, const char* service, const String& payload) const {
    _logger->logf("INFO", "HA Client: Sending command - Entity: %s, Service: %s", entityId, service);
    _logger->logf("DEBUG", "HA Client: Payload: %s", payload.c_str());
}