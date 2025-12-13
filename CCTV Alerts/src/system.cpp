#include "system.h"
#include "config.h"
#include "alerts.h"
#include <WiFi.h>

// Legacy functions for backward compatibility
void flashLed();
void setupBreathingLed();
void startBreathingEffect();
void stopBreathingEffect();
void updateBreathingLed();
void playBeep();
void startFireAlarm();
void stopFireAlarm();
void updateFireAlarm();

void initSystem() {
    Serial.begin(115200);

    // Initialize watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    // Initialize preferences
    preferences.begin("cctv-alerts", false);

    // Track boot count and check for boot loops
    bootCount++;
    if (bootCount > 3 && millis() < 60000) {
        Serial.println("Possible boot loop detected!");
        delay(5000);
    }
    Serial.printf("Boot count: %d\n", bootCount);

    // Record start time for uptime calculation
    startTime = millis();
    lastFreeHeap = ESP.getFreeHeap();
}

void checkSystemHealth() {
    size_t currentFreeHeap = ESP.getFreeHeap();

    // Check for significant heap changes
    if (abs((int)(currentFreeHeap - lastFreeHeap)) > 1024) {
        Serial.printf("Significant heap change detected! Before: %d, After: %d\n",
                     lastFreeHeap, currentFreeHeap);
        logSystemEvent("Heap change: " + String(lastFreeHeap) + " -> " + String(currentFreeHeap));
    }

    // Check for low memory condition
    if (currentFreeHeap < 10000) {
        Serial.println("Warning: Low memory condition detected!");
        logError("Low memory: " + String(currentFreeHeap) + " bytes free");
    }

    lastFreeHeap = currentFreeHeap;
}

void logSystemEvent(const String& event) {
    Serial.println("[EVENT] " + event);
}

void logError(const String& error) {
    Serial.println("[ERROR] " + error);
}

void resetWatchdog() {
    esp_task_wdt_reset();
}

String getSystemStatusJson() {
    String json = "{";
    json += "\"uptime\":" + String(millis() - startTime) + ",";
    json += "\"wifi_strength\":" + String(WiFi.RSSI()) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"boot_count\":" + String(bootCount) + ",";
    json += "\"wifi_connected\":" + String(wifiConnected ? "true" : "false") + ",";
    json += "\"alert_active\":" + String(isAlertActive() ? "true" : "false") + ",";
    json += "\"fire_alarm_active\":" + String(fireAlarmActive ? "true" : "false") + ",";
    json += "\"heap_change\":" + String(ESP.getFreeHeap() - lastFreeHeap);
    json += "}";
    return json;
}

// Legacy LED functions for backward compatibility
void flashLed() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastFlashTime > FLASH_DURATION) {
        lastFlashTime = currentMillis;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}

void setupBreathingLed() {
    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
    ledcAttachPin(LED_PIN, LED_CHANNEL);
}

void startBreathingEffect() {
    setupBreathingLed();
    breathing = true;
    breathingStartTime = millis();
}

void stopBreathingEffect() {
    breathing = false;
    ledcWrite(LED_CHANNEL, 0);
}

void updateBreathingLed() {
    if (!breathing) return;

    unsigned long elapsedTime = millis() - breathingStartTime;
    if (elapsedTime < BREATHING_DURATION) {
        float breathePhase = (elapsedTime % BREATHE_PERIOD) / (float)BREATHE_PERIOD;
        int brightness = (int)((exp(sin(breathePhase * PI)) - 0.36787944) * (255 / exp(1)));
        ledcWrite(LED_CHANNEL, brightness);
    } else {
        stopBreathingEffect();
    }
}

void playBeep() {
    tone(SPEAKER_PIN, 1000, 500);
    delay(500);
    noTone(SPEAKER_PIN);
}

void startFireAlarm() {
    fireAlarmActive = true;
    breathing = false;
    lastFireAlarmTone = 0;
    lastFlashTime = 0;
    digitalWrite(SPEAKER_PIN, LOW);
    noTone(SPEAKER_PIN);
}

void stopFireAlarm() {
    fireAlarmActive = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(SPEAKER_PIN, LOW);
    noTone(SPEAKER_PIN);
}

void updateFireAlarm() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastFlashTime >= FIRE_ALARM_FLASH_DURATION) {
        lastFlashTime = currentMillis;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    if (currentMillis - lastFireAlarmTone >= FIRE_ALARM_TONE_INTERVAL) {
        lastFireAlarmTone = currentMillis;
        static bool highTone = false;
        tone(SPEAKER_PIN, highTone ? 2000 : 1500, FIRE_ALARM_TONE_INTERVAL - 50);
        highTone = !highTone;
    }
}