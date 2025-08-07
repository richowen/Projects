#pragma once
#include <stdint.h>

// Centralized configuration for Milk Machine V5
// All timings in milliseconds unless otherwise noted.

// Pins (FireBeetle ESP32) - active LOW relays, INPUT_PULLUP switches
namespace Pins {
    constexpr int RELAY_AUGER    = 25;
    constexpr int RELAY_AGITATOR = 26;
    constexpr int RELAY_MIXER    = 16;
    constexpr int RELAY_WATER    = 17;

    constexpr int LEVEL_SWITCH   = 12; // Pressure switch, LOW = active
    constexpr int WASH_STANDBY   = 23; // Hold LOW to enter wash
    constexpr int WASH_DISPENSE  = 5;  // LOW to enable water during wash
}

// LCD (DFRobot RGB LCD 1602)
namespace LcdCfg {
    constexpr uint8_t I2C_ADDR = 0x2D;
    constexpr int COLS = 16;
    constexpr int ROWS = 2;

    // Colors
    constexpr uint8_t IDLE_R = 0,   IDLE_G = 0,   IDLE_B = 255;
    constexpr uint8_t MIX_R  = 0,   MIX_G  = 255, MIX_B  = 0;
    constexpr uint8_t ERR_R  = 255, ERR_G  = 0,   ERR_B  = 0;
    constexpr uint8_t WASH_R = 0,   WASH_G = 255, WASH_B = 255;
    constexpr uint8_t BOOT_R = 0,   BOOT_G = 0,   BOOT_B = 200;
    constexpr uint8_t OTA_R  = 255, OTA_G  = 255, OTA_B  = 0;
}

// Timings (ms)
namespace Times {
    constexpr unsigned long MIXING_TIMEOUT         = 60000;   // 60s
    constexpr unsigned long POST_MIX_TIME          = 5000;    // 5s
    constexpr unsigned long ERROR_RETRY_DELAY      = 300000;  // 5min
    constexpr unsigned long WATCHDOG_INTERVAL      = 100;     // 100ms
    constexpr unsigned long LCD_UPDATE_INTERVAL    = 500;     // 500ms
    constexpr unsigned long LEVEL_DEBOUNCE_TIME    = 1000;    // 1s
    constexpr unsigned long WASH_TIMEOUT           = 30000;   // 30s
    constexpr unsigned long DATA_REPORT_INTERVAL   = 60000;   // 60s
    constexpr unsigned long PERIODIC_MIX_INTERVAL  = 300000;  // 5min
    constexpr unsigned long PERIODIC_MIX_DURATION  = 5000;    // 5s
    constexpr unsigned long MQTT_RECONNECT_TIMEOUT = 10000;   // 10s
}

// Networking
namespace Net {
    // WiFi
    static constexpr const char* WIFI_SSID = "WiFi";
    static constexpr const char* WIFI_PASS = "Gliders1!";

    // static IP
    constexpr uint8_t IP[4]      = {192,168,1,16};
    constexpr uint8_t GATE[4]    = {192,168,1,1};
    constexpr uint8_t SUBNET[4]  = {255,255,255,0};
    constexpr uint8_t DNS1[4]    = {8,8,8,8};
    constexpr uint8_t DNS2[4]    = {8,8,4,4};

    // MQTT
    static constexpr const char* MQTT_HOST = "192.168.1.3";
    constexpr int MQTT_PORT = 1883;
    static constexpr const char* MQTT_USER = "richowen";
    static constexpr const char* MQTT_PASS = "p";

    // Topics (as per README)
    static constexpr const char* T_STATUS              = "milk_mixer/status";
    static constexpr const char* T_ERROR               = "milk_mixer/error";
    static constexpr const char* T_AVAILABLE           = "milk_mixer/available";
    static constexpr const char* T_DATA_TOTAL_MIXES    = "milk_mixer/data/total_mixes";
    static constexpr const char* T_DATA_SESSION_MIXES  = "milk_mixer/data/session_mixes";
    static constexpr const char* T_DATA_UPTIME_HOURS   = "milk_mixer/data/uptime_hours";
    static constexpr const char* T_DATA_LAST_MIX       = "milk_mixer/data/last_mix";
    static constexpr const char* T_DATA_ERROR_COUNT    = "milk_mixer/data/error_count";
}

// App constants
namespace AppCfg {
    static constexpr const char* OTA_HOSTNAME = "MilkMixer-OTA";
    static constexpr const char* PREFS_NS     = "milkmixer";
    constexpr uint8_t LCD_CLEAR_LINE_LEN = 16;
}