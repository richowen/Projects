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
    // Note: NVS is not yet initialized at global-constructor time, so we can't
    // reliably load saved input config here. loadInputConfig() is called again
    // from load(), which main.cpp invokes after nvs_flash_init() in setup().
    loadInputConfig();
}



// ========================================
// ICONFIGMANAGER INTERFACE IMPLEMENTATION
// ========================================

bool ConfigManager::load() {
    // Re-load runtime-editable input config from NVS now that NVS flash has
    // been initialized (must be called after nvs_flash_init() in setup()).
    loadInputConfig();
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
    if (strcmp(entityType, "light_scene") == 0) return ENTITY_LIGHT_SCENE;
    if (strcmp(entityType, "lights_brightness") == 0) return ENTITY_LIGHTS_BRIGHTNESS;

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

const char* ConfigManager::getInputEntityId(uint8_t index) const {
    if (index >= MAX_INPUTS) return "";
    return _inputEntityId[index].c_str();
}

const char* ConfigManager::getInputService(uint8_t index) const {
    if (index >= MAX_INPUTS) return "";
    return _inputService[index].c_str();
}

uint8_t ConfigManager::getInputType(uint8_t index) const {
    if (index >= MAX_INPUTS) return 0;
    return _inputType[index];
}

const char* ConfigManager::getInputName(uint8_t index) const {
    static const char* names[] = {
        "AC Power", "AC Bypass", "PC Shutdown", "Lights", "Immersion",
        "Extra 1", "Plex On", "Extra 3", "Extra 4", "Extra 5"
    };
    if (index >= MAX_INPUTS) return "Unknown";
    return names[index];
}

const char* ConfigManager::getInputLabel(uint8_t index) const {
    if (index >= MAX_INPUTS) return "";
    return _inputLabel[index].c_str();
}

bool ConfigManager::setInputConfig(uint8_t index, const char* entityId, const char* service, uint8_t type, const char* label) {
    if (index >= MAX_INPUTS) return false;
    if (entityId == nullptr || service == nullptr) return false;
    if (type > 1) return false;

    _inputEntityId[index] = entityId;
    _inputService[index] = service;
    _inputType[index] = type;
    _inputLabel[index] = (label != nullptr) ? label : "";

    // Persist to NVS
    if (!_prefs.begin("cp_inputs", false)) {
        return false;
    }

    char keyEnt[16], keySvc[16], keyTyp[16], keyLbl[16];
    snprintf(keyEnt, sizeof(keyEnt), "e%d", index);
    snprintf(keySvc, sizeof(keySvc), "s%d", index);
    snprintf(keyTyp, sizeof(keyTyp), "t%d", index);
    snprintf(keyLbl, sizeof(keyLbl), "l%d", index);

    _prefs.putString(keyEnt, _inputEntityId[index]);
    _prefs.putString(keySvc, _inputService[index]);
    _prefs.putUChar(keyTyp, _inputType[index]);
    _prefs.putString(keyLbl, _inputLabel[index]);

    _prefs.end();
    return true;
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

// ========================================
// RUNTIME-EDITABLE INPUT CONFIGURATION
// ========================================

const char* ConfigManager::getDefaultInputEntityId(uint8_t index) const {
    switch (index) {
        case 0: return ENTITY_AC_UNIT;
        case 1: return ENTITY_AC_BYPASS;
        case 2: return ENTITY_PC_SHUTDOWN;
        case 3: return ENTITY_LIGHTS;
        case 4: return ENTITY_IMMERSION;
        case 5: return ENTITY_EXTRA_1;
        case 6: return ENTITY_PLEX;
        case 7: return ENTITY_EXTRA_3;
        case 8: return ENTITY_EXTRA_4;
        case 9: return ENTITY_EXTRA_5;
        default: return "";
    }
}

const char* ConfigManager::getDefaultInputService(uint8_t index) const {
    switch (index) {
        case 0: return "toggle";
        case 1: return "input_boolean";
        case 2: return "trigger";
        case 3: return "trigger";
        case 4: return "switch";
        case 5: return "toggle";
        case 6: return "trigger";
        case 7: return "toggle";
        case 8: return "toggle";
        case 9: return "switch";
        default: return "toggle";
    }
}

uint8_t ConfigManager::getDefaultInputType(uint8_t index) const {
    // 0 = MOMENTARY_BUTTON, 1 = TOGGLE_SWITCH
    switch (index) {
        case 1: return 1; // AC Bypass - toggle switch
        case 4: return 1; // Immersion - toggle switch
        case 9: return 1; // Extra 5 - toggle switch
        default: return 0; // All others momentary
    }
}


void ConfigManager::loadInputConfig() {
    bool hasPrefs = _prefs.begin("cp_inputs", true); // read-only

    for (uint8_t i = 0; i < MAX_INPUTS; i++) {
        char keyEnt[16], keySvc[16], keyTyp[16], keyLbl[16];
        snprintf(keyEnt, sizeof(keyEnt), "e%d", i);
        snprintf(keySvc, sizeof(keySvc), "s%d", i);
        snprintf(keyTyp, sizeof(keyTyp), "t%d", i);
        snprintf(keyLbl, sizeof(keyLbl), "l%d", i);

        if (hasPrefs && _prefs.isKey(keyEnt)) {
            _inputEntityId[i] = _prefs.getString(keyEnt, getDefaultInputEntityId(i));
            _inputService[i] = _prefs.getString(keySvc, getDefaultInputService(i));
            _inputType[i] = _prefs.getUChar(keyTyp, getDefaultInputType(i));
            _inputLabel[i] = _prefs.getString(keyLbl, "");
        } else {
            _inputEntityId[i] = getDefaultInputEntityId(i);
            _inputService[i] = getDefaultInputService(i);
            _inputType[i] = getDefaultInputType(i);
            _inputLabel[i] = "";
        }
    }


    if (hasPrefs) {
        _prefs.end();
    }
}


