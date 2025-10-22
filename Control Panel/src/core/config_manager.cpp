#include "config_manager.h"

// ========================================
// CONSTANTS (matching main.cpp defines)
// ========================================

#define ADC_MAX_VALUE 4095           // ESP32 12-bit ADC maximum value
#define WIFI_CONNECT_ATTEMPTS 30     // Maximum WiFi connection attempts
#define HTTP_TIMEOUT_MS 5000         // HTTP request timeout (5 seconds)
#define POT_SAMPLES 10               // Potentiometer averaging samples
#define POT_STABLE_DURATION 500      // Value must be stable for 500ms before sending
#define PC_SHUTDOWN_HOLD_TIME 3000   // 3 seconds hold required
#define PLEX_ON_HOLD_TIME 3000       // 3 seconds hold required
#define WIFI_RETRY_INTERVAL 30000    // 30 seconds between reconnection attempts
#define DISPLAY_TIMEOUT_MS 5000      // Clear display after 5 seconds
#define DISPLAY_HEIGHT 8             // LED matrix height in pixels

// ========================================
// CONSTRUCTOR
// ========================================

ConfigManager::ConfigManager() {
    // Constructor - configuration is loaded from config.h via preprocessor
}

// ========================================
// ICONFIGMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool ConfigManager::load() {
    // For now, configuration is compile-time only
    // Future enhancement: load from EEPROM/SD card
    return validateConfig();
}

bool ConfigManager::save() {
    // For now, configuration is read-only
    // Future enhancement: save to EEPROM/SD card
    return true;
}

const char* ConfigManager::getWiFiSSID() const {
    return WIFI_SSID;
}

const char* ConfigManager::getWiFiPassword() const {
    return WIFI_PASSWORD;
}

const char* ConfigManager::getHAURL() const {
    return HA_URL;
}

const char* ConfigManager::getHAToken() const {
    return HA_TOKEN;
}

void ConfigManager::getTemperatureRange(int& minTemp, int& maxTemp) const {
    minTemp = TEMP_MIN;
    maxTemp = TEMP_MAX;
}

int ConfigManager::getPin(const char* pinType) const {
    if (strcmp(pinType, "display_cs") == 0) return getDisplayCSPin();
    if (strcmp(pinType, "pot_ac_temp") == 0) return getPotPin();
    if (strcmp(pinType, "status_led") == 0) return getStatusLEDPin();

    // Button pins
    if (strncmp(pinType, "button_", 7) == 0) {
        int buttonIndex = atoi(pinType + 7);
        if (buttonIndex >= 0 && buttonIndex < 10) {
            return getButtonPin(buttonIndex);
        }
    }

    return -1; // Invalid pin type
}

unsigned long ConfigManager::getTiming(const char* timingType) const {
    if (strcmp(timingType, "debounce_delay") == 0) return getDebounceDelay();
    if (strcmp(timingType, "wifi_timeout") == 0) return getWiFiTimeout();
    if (strcmp(timingType, "http_timeout") == 0) return getHTTPTimeout();
    if (strcmp(timingType, "pot_stability") == 0) return getPotStabilityDuration();
    if (strcmp(timingType, "pc_shutdown_hold") == 0) return getPCShutdownHoldTime();
    if (strcmp(timingType, "plex_on_hold") == 0) return getPlexOnHoldTime();
    if (strcmp(timingType, "wifi_retry") == 0) return getWiFiRetryInterval();
    if (strcmp(timingType, "display_timeout") == 0) return getDisplayTimeout();

    return 0; // Invalid timing type
}

const char* ConfigManager::getEntityId(const char* entityType) const {
    if (strcmp(entityType, "ac_unit") == 0) return ENTITY_AC_UNIT;
    if (strcmp(entityType, "ac_temp") == 0) return ENTITY_AC_TEMP;
    if (strcmp(entityType, "ac_bypass") == 0) return ENTITY_AC_BYPASS;
    if (strcmp(entityType, "ac_automation") == 0) return ENTITY_AC_AUTOMATION;
    if (strcmp(entityType, "pc_shutdown") == 0) return ENTITY_PC_SHUTDOWN;
    if (strcmp(entityType, "plex") == 0) return ENTITY_PLEX;
    if (strcmp(entityType, "lights") == 0) return ENTITY_LIGHTS;
    if (strcmp(entityType, "immersion") == 0) return ENTITY_IMMERSION;

    // Extra buttons
    if (strncmp(entityType, "extra_", 6) == 0) {
        int extraIndex = atoi(entityType + 6);
        switch (extraIndex) {
            case 1: return ENTITY_EXTRA_1;
            case 2: return ENTITY_EXTRA_2;
            case 3: return ENTITY_EXTRA_3;
            case 4: return ENTITY_EXTRA_4;
            case 5: return ENTITY_EXTRA_5;
        }
    }

    return ""; // Invalid entity type
}

// ========================================
// ADDITIONAL CONFIGURATION METHODS
// ========================================

bool ConfigManager::isDebugMode() const {
    return DEBUG_MODE;
}

uint8_t ConfigManager::getDisplayIntensity() const {
    return DISPLAY_INTENSITY;
}

unsigned long ConfigManager::getDebounceDelay() const {
    return DEBOUNCE_DELAY;
}

int ConfigManager::getADCMaxValue() const {
    return ADC_MAX_VALUE;
}

unsigned long ConfigManager::getWiFiTimeout() const {
    return WIFI_CONNECT_ATTEMPTS * 500; // attempts * 500ms per attempt
}

unsigned long ConfigManager::getHTTPTimeout() const {
    return HTTP_TIMEOUT_MS;
}

uint8_t ConfigManager::getPotSamples() const {
    return POT_SAMPLES;
}

unsigned long ConfigManager::getPotStabilityDuration() const {
    return POT_STABLE_DURATION;
}

unsigned long ConfigManager::getPCShutdownHoldTime() const {
    return PC_SHUTDOWN_HOLD_TIME;
}

unsigned long ConfigManager::getPlexOnHoldTime() const {
    return PLEX_ON_HOLD_TIME;
}

unsigned long ConfigManager::getWiFiRetryInterval() const {
    return WIFI_RETRY_INTERVAL;
}

unsigned long ConfigManager::getDisplayTimeout() const {
    return DISPLAY_TIMEOUT_MS;
}

uint8_t ConfigManager::getDisplayHeight() const {
    return DISPLAY_HEIGHT;
}

// ========================================
// PRIVATE METHODS
// ========================================

bool ConfigManager::validateConfig() const {
    // Basic validation of configuration values
    if (TEMP_MIN >= TEMP_MAX) return false;
    if (DISPLAY_INTENSITY > 15) return false;
    if (POT_SAMPLES == 0) return false;

    // Validate pins are in valid ranges for ESP32
    int pins[] = {
        DISPLAY_CS_PIN, POT_AC_TEMP_PIN, STATUS_LED_PIN,
        BTN_AC_POWER_PIN, BTN_AC_BYPASS_PIN, BTN_PC_OFF_PIN,
        BTN_LIGHTS_PIN, BTN_IMMERSION_PIN, BTN_EXTRA_1_PIN,
        BTN_EXTRA_2_PIN, BTN_EXTRA_3_PIN, BTN_EXTRA_4_PIN, BTN_EXTRA_5_PIN
    };

    for (int pin : pins) {
        if (pin < 0 || pin > 39) return false; // ESP32 GPIO range
    }

    return true;
}

int ConfigManager::getDisplayCSPin() const {
    return DISPLAY_CS_PIN;
}

int ConfigManager::getPotPin() const {
    return POT_AC_TEMP_PIN;
}

int ConfigManager::getButtonPin(uint8_t buttonIndex) const {
    switch (buttonIndex) {
        case 0: return BTN_AC_POWER_PIN;
        case 1: return BTN_AC_BYPASS_PIN;
        case 2: return BTN_PC_OFF_PIN;
        case 3: return BTN_LIGHTS_PIN;
        case 4: return BTN_IMMERSION_PIN;
        case 5: return BTN_EXTRA_1_PIN;
        case 6: return BTN_EXTRA_2_PIN;
        case 7: return BTN_EXTRA_3_PIN;
        case 8: return BTN_EXTRA_4_PIN;
        case 9: return BTN_EXTRA_5_PIN;
        default: return -1;
    }
}

int ConfigManager::getStatusLEDPin() const {
    return STATUS_LED_PIN;
}