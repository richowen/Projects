#include "App.h"

using Timers::elapsed;
using Timers::now;

void App::begin() {
    Serial.begin(115200);

    // Hardware
    relays.begin();
    inputs.begin();
    lcd.begin();
    store.begin();

    // Networking (non-blocking critical ops)
    wifi.begin(&lcd);
    mqtt.begin(&store);

    // OTA manager init (will only be begun in safe states)
    ota.init(&lcd);

    // Initial UI
    lcd.showIdle();

    // Initial timers
    tWatchdog = now();
    tLcd = now();
    tPeriodicMix = now();
    periodicMixActive = false;
    washWaterActive = false;

    state = SystemState::IDLE;
    error = ErrorType::NONE;

    // Initial MQTT status if already connected
    if (mqtt.connected()) {
        mqtt.onEnterIdle();
        mqtt.publishData();
    }
}

void App::tick() {
    // Network service
    wifi.loop();
    mqtt.loop();

    // OTA should only run when in safe states (IDLE or WASH)
    if (state == SystemState::IDLE || state == SystemState::WASH) {
        ota.beginWhenSafe();
        ota.loop();
    } else {
        ota.pause();
    }

    // Poll inputs and handle wash mode first
    inputs.poll();

    if (inputs.washStandbyActive()) {
        if (state != SystemState::WASH) {
            enterWash();
        }
        bool shouldWater = inputs.washDispenseActive();
        if (shouldWater && !washWaterActive) {
            relays.waterOn();
            washWaterActive = true;
            tWashWaterStart = now();
        } else if (!shouldWater && washWaterActive) {
            relays.waterOff();
            washWaterActive = false;
        } else if (washWaterActive && elapsed(tWashWaterStart, Times::WASH_TIMEOUT)) {
            // Safety timeout
            relays.waterOff();
            washWaterActive = false;
        }
    } else if (state == SystemState::WASH) {
        exitWash();
    }

    // Periodic LCD refresh
    if (elapsed(tLcd, Times::LCD_UPDATE_INTERVAL)) {
        updateLCD();
        tLcd = now();
    }

    // Watchdog
    if (elapsed(tWatchdog, Times::WATCHDOG_INTERVAL)) {
        watchdogCheck();
        tWatchdog = now();
    }

    // State machine
    switch (state) {
        case SystemState::IDLE: {
            // Periodic mixing: mixer only
            if (!periodicMixActive && elapsed(tPeriodicMix, Times::PERIODIC_MIX_INTERVAL)) {
                relays.mixerOn();
                periodicMixActive = true;
                tPeriodicMixStart = now();
                tPeriodicMix = now();
            }
            if (periodicMixActive && elapsed(tPeriodicMixStart, Times::PERIODIC_MIX_DURATION)) {
                relays.mixerOff();
                periodicMixActive = false;
            }

            // Automatic start when level switch activates (water level low)
            if (inputs.levelActive()) {
                // Stop any periodic mixing before starting full cycle
                if (periodicMixActive) {
                    relays.mixerOff();
                    periodicMixActive = false;
                }
                enterMixing();
                // For visibility, publish status immediately if MQTT connected
                if (mqtt.connected()) mqtt.onEnterMixing();
            }
            break;
        }

        case SystemState::MIXING: {
            if (elapsed(tMixStart, Times::MIXING_TIMEOUT)) {
                enterError(ErrorType::TIMEOUT);
                break;
            }

            // When level restores (switch inactive), go to post-mixing
            if (!inputs.levelActive()) {
                enterPostMixing();
                if (mqtt.connected()) mqtt.onEnterPostMix();
            }
            break;
        }

        case SystemState::POST_MIXING: {
            if (elapsed(tPostMixStart, Times::POST_MIX_TIME)) {
                enterIdle();
                // Mix cycle complete: record metrics and publish
                store.onMixCompleted();
                if (mqtt.connected()) {
                    mqtt.onEnterIdle();
                    mqtt.publishData();
                }
            }
            break;
        }

        case SystemState::ERROR: {
            if (elapsed(tErrorStart, Times::ERROR_RETRY_DELAY)) {
                // Recover to idle
                error = ErrorType::NONE;
                enterIdle();
                if (mqtt.connected()) mqtt.onEnterIdle();
            }
            break;
        }

        case SystemState::WASH:
            // handled above
            break;
    }
}

void App::enterIdle() {
    relays.allOff();
    state = SystemState::IDLE;
    updateLCD();
    if (mqtt.connected()) mqtt.onEnterIdle();
}

void App::enterMixing() {
    relays.allOn();
    state = SystemState::MIXING;
    tMixStart = now();
    updateLCD();
    if (mqtt.connected()) mqtt.onEnterMixing();
}

void App::enterPostMixing() {
    // Turn off all except mixer
    relays.augerOff();
    relays.agitatorOff();
    relays.waterOff();
    // mixer stays on from previous
    state = SystemState::POST_MIXING;
    tPostMixStart = now();
    updateLCD();
    if (mqtt.connected()) mqtt.onEnterPostMix();
}

void App::enterError(ErrorType e) {
    relays.allOff();
    state = SystemState::ERROR;
    error = e;
    tErrorStart = now();
    // persist error metric
    store.onError();
    updateLCD();

    if (mqtt.connected()) {
        switch (e) {
            case ErrorType::TIMEOUT:       mqtt.onEnterErrorTimeout(); break;
            case ErrorType::LEVEL_SWITCH:  mqtt.onEnterErrorLevelSwitch(); break;
            default:                       mqtt.onEnterErrorUnknown(); break;
        }
        mqtt.publishData();
    }
}

void App::enterWash() {
    // ensure all except water are off; water is controlled by input
    relays.augerOff();
    relays.agitatorOff();
    relays.mixerOff();
    relays.waterOff();
    washWaterActive = false;
    state = SystemState::WASH;
    updateLCD();
    if (mqtt.connected()) mqtt.onEnterWash();
}

void App::exitWash() {
    relays.waterOff();
    washWaterActive = false;
    state = SystemState::IDLE;
    updateLCD();
    if (mqtt.connected()) mqtt.onEnterIdle();
}

void App::updateLCD() {
    switch (state) {
        case SystemState::IDLE:
            lcd.showIdle();
            break;
        case SystemState::MIXING:
            lcd.showMixing();
            break;
        case SystemState::POST_MIXING: {
            unsigned long elapsedMs = now() - tPostMixStart;
            unsigned long remainMs = (elapsedMs >= Times::POST_MIX_TIME) ? 0 : (Times::POST_MIX_TIME - elapsedMs);
            uint16_t sec = static_cast<uint16_t>(remainMs / 1000ul);
            lcd.showPostMix(sec);
            break;
        }
        case SystemState::ERROR:
            lcd.showError(error == ErrorType::TIMEOUT ? "Timeout Error!" :
                          error == ErrorType::LEVEL_SWITCH ? "Level Sw Error!" : "Unknown Error!");
            break;
        case SystemState::WASH:
            lcd.showWash(washWaterActive);
            break;
    }
}

void App::watchdogCheck() {
    // During mixing, if level input hasn't changed for 60s, count as switch issue.
    static bool lastLevel = false;
    static unsigned long tStable = 0;

    bool lvl = inputs.levelActive();
    if (state == SystemState::MIXING) {
        if (lvl == lastLevel) {
            if (tStable == 0) {
                tStable = now();
            } else if (elapsed(tStable, 60000)) { // 60s stuck
                enterError(ErrorType::LEVEL_SWITCH);
                tStable = 0;
            }
        } else {
            tStable = 0;
        }
    } else {
        tStable = 0;
    }
    lastLevel = lvl;
}