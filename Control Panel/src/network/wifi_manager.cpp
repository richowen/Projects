#include "wifi_manager.h"

// ========================================
// CONSTRUCTOR
// ========================================

WiFiManager::WiFiManager(IConfigManager* config, ILogger* logger)
    : _config(config), _logger(logger), _lastReconnectAttempt(0), _wasConnected(false) {
}

// ========================================
// IWIFIMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool WiFiManager::begin() {
    _logger->info("WiFi Manager: Initializing...");

    if (_config->isDebugMode()) {
        _logger->info("WiFi Manager: Debug mode - skipping WiFi connection");
        return true;
    }

    return connect();
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

uint8_t WiFiManager::getSignalStrength() const {
    if (!isConnected()) return 0;

    int rssi = WiFi.RSSI();
    return rssiToSignalLevel(rssi);
}

String WiFiManager::getIPAddress() const {
    if (!isConnected()) return "0.0.0.0";

    return WiFi.localIP().toString();
}

bool WiFiManager::reconnect() {
    if (isConnected()) return true;

    _logger->info("WiFi Manager: Attempting reconnection...");
    _lastReconnectAttempt = millis();
    return connect();
}

void WiFiManager::update() {
    if (_config->isDebugMode()) return;

    bool currentlyConnected = isConnected();

    // Log connection state changes
    if (currentlyConnected != _wasConnected) {
        if (currentlyConnected) {
            _logger->info("WiFi Manager: Connected to network");
            _logger->logf("INFO", "WiFi Manager: IP Address: %s", getIPAddress().c_str());
            _logger->logf("INFO", "WiFi Manager: Signal strength: %d/3", getSignalStrength());
        } else {
            _logger->warning("WiFi Manager: Lost connection to network");
        }
        _wasConnected = currentlyConnected;
    }

    // Attempt reconnection if disconnected and enough time has passed
    if (!currentlyConnected && shouldAttemptReconnect()) {
        reconnect();
    }
}

// ========================================
// PRIVATE METHODS
// ========================================

bool WiFiManager::connect() {
    if (_config->isDebugMode()) return true;

    const char* ssid = _config->getWiFiSSID();
    const char* password = _config->getWiFiPassword();

    _logger->logf("INFO", "WiFi Manager: Connecting to '%s'...", ssid);

    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    unsigned long timeout = _config->getWiFiTimeout();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeout) {
        delay(500);
        attempts++;

        // Log progress every 5 attempts
        if (attempts % 5 == 0) {
            _logger->logf("DEBUG", "WiFi Manager: Connection attempt %d...", attempts);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        _logger->info("WiFi Manager: Successfully connected!");
        _wasConnected = true;
        return true;
    } else {
        _logger->error("WiFi Manager: Connection failed - timeout");
        _wasConnected = false;
        return false;
    }
}

uint8_t WiFiManager::rssiToSignalLevel(int rssi) const {
    if (rssi >= -50) return 3;  // Excellent
    if (rssi >= -60) return 2;  // Good
    if (rssi >= -70) return 1;  // Fair
    return 0;                   // Poor
}

bool WiFiManager::shouldAttemptReconnect() const {
    unsigned long timeSinceLastAttempt = millis() - _lastReconnectAttempt;
    unsigned long retryInterval = _config->getWiFiRetryInterval();

    return timeSinceLastAttempt >= retryInterval;
}