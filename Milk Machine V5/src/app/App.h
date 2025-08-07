#pragma once
#include <Arduino.h>
#include "../core/Config.h"
#include "../core/Timers.h"
#include "../core/DataStore.h"
#include "../hal/Relays.h"
#include "../hal/Inputs.h"
#include "../hal/Lcd.h"
#include "../net/WiFiManager.h"
#include "../net/MqttClient.h"
#include "../net/OtaManager.h"

enum class SystemState : uint8_t {
    IDLE,
    MIXING,
    POST_MIXING,
    ERROR,
    WASH
};

enum class ErrorType : uint8_t {
    NONE,
    TIMEOUT,
    LEVEL_SWITCH
};

class App {
public:
    void begin();
    void tick();

private:
    // Modules
    HAL::Relays relays{};
    HAL::Inputs inputs{};
    HAL::Lcd lcd{};
    DataStore store{};
    WiFiManagerMM wifi{};
    MqttClientMM mqtt{};
    OtaManagerMM ota{};

    // State
    SystemState state{SystemState::IDLE};
    ErrorType error{ErrorType::NONE};

    // Timing
    unsigned long tMixStart{0};
    unsigned long tPostMixStart{0};
    unsigned long tErrorStart{0};
    unsigned long tWatchdog{0};
    unsigned long tLcd{0};
    unsigned long tPeriodicMix{0};
    unsigned long tPeriodicMixStart{0};

    // Flags
    bool periodicMixActive{false};
    bool washWaterActive{false};
    unsigned long tWashWaterStart{0};

    // Behavior helpers
    void enterIdle();
    void enterMixing();
    void enterPostMixing();
    void enterError(ErrorType e);
    void enterWash();
    void exitWash();

    void updateLCD();
    void watchdogCheck();
};