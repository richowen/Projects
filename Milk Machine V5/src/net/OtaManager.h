#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h>
#include "../core/Config.h"
#include "../hal/Lcd.h"

// Guarded OTA: only active when system is in safe states (IDLE/WASH) as controlled by caller.
// Caller must call beginWhenSafe() from a safe state and loop() periodically while safe.

class OtaManagerMM {
public:
    void init(HAL::Lcd* lcdPtr) {
        lcd = lcdPtr;
        ArduinoOTA.setHostname(AppCfg::OTA_HOSTNAME);

        ArduinoOTA.onStart([this]() {
            otaActive = true;
            if (lcd) {
                lcd->clear();
                lcd->setOtaColor();
                lcd->print(0, 0, "OTA Update...");
                lcd->print(0, 1, "Starting");
            }
            Serial.println("OTA: Start");
        });

        ArduinoOTA.onEnd([this]() {
            if (lcd) {
                lcd->print(0, 1, "Complete!");
            }
            Serial.println("OTA: End");
            otaActive = false;
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            if (!lcd) return;
            char buf[17];
            uint8_t pct = total ? (progress * 100U / total) : 0;
            snprintf(buf, sizeof(buf), "Progress: %3u%%", pct);
            lcd->print(0, 1, buf);
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            if (lcd) {
                lcd->print(0, 1, "OTA Error");
            }
            Serial.printf("OTA Error[%u]\n", error);
            otaActive = false;
        });
    }

    // Begin OTA only when in safe states; can be called repeatedly from safe states, will init once
    void beginWhenSafe() {
        if (!initialized) {
            ArduinoOTA.begin();
            initialized = true;
            Serial.println("OTA: Ready");
        }
    }

    // Call while in safe states
    void loop() {
        if (initialized) {
            ArduinoOTA.handle();
        }
    }

    // Call when leaving safe state to ensure OTA isn't handled during critical ops
    void pause() {
        // We don't have a stop() in ArduinoOTA; simply stop calling handle() by leaving initialized true.
        // Optionally, could set a flag here to suppress UI.
    }

    bool isActive() const { return otaActive; }

private:
    HAL::Lcd* lcd{nullptr};
    bool initialized{false};
    bool otaActive{false};
};