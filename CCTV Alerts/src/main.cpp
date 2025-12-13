#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <math.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>

// Include our modular headers
#include "config.h"
#include "alerts.h"
#include "network.h"
#include "system.h"

// Forward declarations for legacy functions
void updateFireAlarm();
void flashLed();
void updateBreathingLed();

void setup() {
    initSystem();
    initAlerts();
    initNetwork();

    // Create WiFi connection task
    xTaskCreatePinnedToCore(
        [] (void * pvParameters) {
            for (;;) {
                connectToWifi();
                resetWatchdog();
                vTaskDelay(currentReconnectInterval / portTICK_PERIOD_MS);
            }
        },
        "WiFiTask",
        4096,
        NULL,
        1,
        &wifiTaskHandle,
        1);

    // Create system health monitoring task
    xTaskCreatePinnedToCore(
        [] (void * pvParameters) {
            for (;;) {
                checkSystemHealth();
                resetWatchdog();
                vTaskDelay(HEAP_CHECK_INTERVAL / portTICK_PERIOD_MS);
            }
        },
        "HealthTask",
        4096,
        NULL,
        1,
        &healthTaskHandle,
        0);
}

void loop() {
    resetWatchdog();

    if (fireAlarmActive) {
        updateFireAlarm();
    } else if (isAlertActive()) {
        updateAlerts();
    } else if (!wifiConnected) {
        flashLed();
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    updateBreathingLed();

    vTaskDelay(10 / portTICK_PERIOD_MS);
}
