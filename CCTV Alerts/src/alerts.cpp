#include "alerts.h"
#include "config.h"
#include <Preferences.h>

// Global alert state
static bool alertActive = false;
static AlertType currentAlertType = ALERT_UNKNOWN;
static unsigned long alertStartTime = 0;
static int currentSoundIndex = 0;
static unsigned long lastSoundTime = 0;

// Default alert configurations
static AlertConfig defaultConfigs[ALERT_COUNT] = {
    // ALERT_CAR
    {
        "Car",
        3, // blink
        3000, // 3 seconds
        {1000, 1500, 2000, 0, 0}, // rising tones
        {500, 500, 500, 0, 0},
        3,
        2, //high priority
    },
    // ALERT_TRUCK
    {
        "Truck",
        2, // breathe
        5000, // 5 seconds
        {400, 600, 400, 600, 0}, // alternating low tones
        {300, 300, 300, 300, 0},
        4,
        2 // high priority
    },
    // ALERT_MOTORCYCLE
    {
        "Motorcycle",
        3, // strobe
        2000, // 2 seconds
        {1500, 1800, 0, 0, 0}, // high pitched
        {150, 150, 0, 0, 0},
        2,
        1 // medium priority
    },
    // ALERT_PEDESTRIAN
    {
        "Pedestrian",
        0, // solid
        4000, // 4 seconds
        {1000, 0, 1000, 0, 1000}, // beeps
        {100, 100, 100, 100, 100},
        5,
        0 // low priority
    },
    // ALERT_UNKNOWN
    {
        "Unknown",
        1, // blink
        2500, // 2.5 seconds
        {500, 700, 900, 0, 0}, // varied tones
        {250, 250, 250, 0, 0},
        3,
        1 // medium priority
    }
};

// Current alert configurations (loaded from preferences)
static AlertConfig currentConfigs[ALERT_COUNT];

void initAlerts() {
    // Load configurations from preferences
    loadAlertConfigs();

    // Setup LED pin as regular output
    // Note: PWM will ONLY be used for breathe pattern to avoid tone() conflicts
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Setup speaker pin
    pinMode(SPEAKER_PIN, OUTPUT);
    digitalWrite(SPEAKER_PIN, LOW);
}

void triggerAlert(AlertType type) {
    if (type >= ALERT_COUNT || fireAlarmActive) return;

    // Stop any current alert
    stopAllAlerts();

    currentAlertType = type;
    alertActive = true;
    alertStartTime = millis();
    currentSoundIndex = 0;
    lastSoundTime = millis();
    
    // Start playing the first sound immediately
    AlertConfig& config = currentConfigs[type];
    if (config.soundCount > 0 && config.soundSequence[0] > 0) {
        tone(SPEAKER_PIN, config.soundSequence[0], config.soundDurations[0] - 10);
    }

    Serial.printf("Alert triggered: %s\n", currentConfigs[type].name.c_str());
}

void updateAlerts() {
    if (!alertActive || fireAlarmActive) return;

    unsigned long currentTime = millis();
    AlertConfig& config = currentConfigs[currentAlertType];

    // Check if alert duration has expired
    if (currentTime - alertStartTime >= config.ledDuration) {
        stopAllAlerts();
        return;
    }

    // Update LED pattern
    updateLedPattern(config.ledPattern, currentTime - alertStartTime);

    // Update sound sequence
    updateSoundSequence(config, currentTime);
}

void updateLedPattern(int pattern, unsigned long elapsedTime) {
    switch (pattern) {
        case 0: // solid - use digitalWrite to avoid PWM conflict with tone()
            digitalWrite(LED_PIN, HIGH);
            break;
        case 1: // blink - use digitalWrite
            digitalWrite(LED_PIN, (elapsedTime / 250) % 2 ? HIGH : LOW);
            break;
        case 2: // breathe - needs PWM, setup dynamically
            {
                static bool pwmSetup = false;
                if (!pwmSetup) {
                    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
                    ledcAttachPin(LED_PIN, LED_CHANNEL);
                    pwmSetup = true;
                }
                float phase = (elapsedTime % BREATHE_PERIOD) / (float)BREATHE_PERIOD;
                int brightness = (int)((exp(sin(phase * PI)) - 0.36787944) * (255 / exp(1)));
                ledcWrite(LED_CHANNEL, brightness);
            }
            break;
        case 3: // strobe - use digitalWrite for fast on/off
            digitalWrite(LED_PIN, (elapsedTime / 100) % 2 ? HIGH : LOW);
            break;
    }
}

void updateSoundSequence(AlertConfig& config, unsigned long currentTime) {
    // Check if we've finished all sounds
    if (currentSoundIndex >= config.soundCount) return;

    // Check if current sound duration has elapsed
    if (currentTime - lastSoundTime >= config.soundDurations[currentSoundIndex]) {
        // Stop current tone
        noTone(SPEAKER_PIN);
        
        // Move to next sound
        currentSoundIndex++;
        lastSoundTime = currentTime;

        // Play next sound if available
        if (currentSoundIndex < config.soundCount) {
            int freq = config.soundSequence[currentSoundIndex];
            if (freq > 0) {
                tone(SPEAKER_PIN, freq, config.soundDurations[currentSoundIndex] - 10);
            }
        }
    }
}

void stopAllAlerts() {
    alertActive = false;
    currentAlertType = ALERT_UNKNOWN;
    
    // Turn off LED (works for both digitalWrite and PWM modes)
    digitalWrite(LED_PIN, LOW);
    ledcWrite(LED_CHANNEL, 0);
    
    // Stop sound
    noTone(SPEAKER_PIN);
    digitalWrite(SPEAKER_PIN, LOW);
}

bool isAlertActive() {
    return alertActive;
}

AlertConfig getAlertConfig(AlertType type) {
    if (type >= ALERT_COUNT) return defaultConfigs[ALERT_UNKNOWN];
    return currentConfigs[type];
}

void setAlertConfig(AlertType type, AlertConfig config) {
    if (type >= ALERT_COUNT) return;
    currentConfigs[type] = config;
}

void saveAlertConfigs() {
    preferences.begin("alerts", false);
    for (int i = 0; i < ALERT_COUNT; i++) {
        String key = "alert_" + String(i);
        preferences.putString((key + "_name").c_str(), currentConfigs[i].name);
        preferences.putInt((key + "_pattern").c_str(), currentConfigs[i].ledPattern);
        preferences.putInt((key + "_duration").c_str(), currentConfigs[i].ledDuration);
        preferences.putInt((key + "_priority").c_str(), currentConfigs[i].priority);
        preferences.putInt((key + "_sound_count").c_str(), currentConfigs[i].soundCount);

        for (int j = 0; j < 5; j++) {
            preferences.putInt((key + "_sound_" + String(j)).c_str(), currentConfigs[i].soundSequence[j]);
            preferences.putInt((key + "_dur_" + String(j)).c_str(), currentConfigs[i].soundDurations[j]);
        }
    }
    preferences.end();
}

void loadAlertConfigs() {
    preferences.begin("alerts", true);
    bool loaded = false;

    for (int i = 0; i < ALERT_COUNT; i++) {
        String key = "alert_" + String(i);
        String name = preferences.getString((key + "_name").c_str(), "");
        if (name.length() > 0) {
            loaded = true;
            currentConfigs[i].name = name;
            currentConfigs[i].ledPattern = preferences.getInt((key + "_pattern").c_str(), defaultConfigs[i].ledPattern);
            currentConfigs[i].ledDuration = preferences.getInt((key + "_duration").c_str(), defaultConfigs[i].ledDuration);
            currentConfigs[i].priority = preferences.getInt((key + "_priority").c_str(), defaultConfigs[i].priority);
            currentConfigs[i].soundCount = preferences.getInt((key + "_sound_count").c_str(), defaultConfigs[i].soundCount);

            for (int j = 0; j < 5; j++) {
                currentConfigs[i].soundSequence[j] = preferences.getInt((key + "_sound_" + String(j)).c_str(), defaultConfigs[i].soundSequence[j]);
                currentConfigs[i].soundDurations[j] = preferences.getInt((key + "_dur_" + String(j)).c_str(), defaultConfigs[i].soundDurations[j]);
            }
        } else {
            currentConfigs[i] = defaultConfigs[i];
        }
    }

    preferences.end();

    if (!loaded) {
        Serial.println("Using default alert configurations");
    } else {
        Serial.println("Loaded alert configurations from preferences");
    }
}