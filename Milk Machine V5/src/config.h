#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <cstddef>

// ================= Hardware Pin Configuration =================
struct HardwarePins {
    static constexpr int RELAY_AUGER = 25;
    static constexpr int RELAY_AGITATOR = 26;
    static constexpr int RELAY_MIXER = 16;
    static constexpr int RELAY_WATER = 17;

    static constexpr int LEVEL_SWITCH = 27;
    static constexpr int WASH_STANDBY_PIN = 23;
    static constexpr int WASH_DISPENSE_PIN = 5;
};

// ================= Network Configuration =================
struct NetworkConfig {
    static constexpr const char* WIFI_SSID = "WiFi";
    static constexpr const char* WIFI_PASSWORD = "Gliders1!";

    static constexpr const char* STATIC_IP = "192.168.1.16";
    static constexpr const char* GATEWAY = "192.168.1.1";
    static constexpr const char* SUBNET = "255.255.255.0";
    static constexpr const char* DNS1 = "1.1.1.1";
    static constexpr const char* DNS2 = "8.8.8.8";
};

// ================= MQTT Configuration =================
struct MQTTConfig {
    static constexpr const char* SERVER = "192.168.1.3";
    static constexpr int PORT = 1883;
    static constexpr const char* USER = "richowen";
    static constexpr const char* PASSWORD = "p";
    static constexpr const char* BASE_TOPIC = "home/milk_machine";
    static constexpr unsigned long PUBLISH_INTERVAL_MS = 30UL * 1000UL;
};

// ================= Timing Configuration =================
struct TimingConfig {
    static constexpr unsigned long POST_MIX_DURATION_MS = 5UL * 1000UL;
    static constexpr unsigned long PERIODIC_MIX_DURATION_MS = 5UL * 1000UL;
    static constexpr unsigned long PERIODIC_MIX_INTERVAL_MS = 5UL * 60UL * 1000UL;
    static constexpr unsigned long DEBOUNCE_STABLE_MS = 50;
    static constexpr unsigned long WDT_TIMEOUT_SECONDS = 10;
    static constexpr unsigned long STATUS_INTERVAL_MS = 10UL * 1000UL;
    static constexpr unsigned long FAULT_COOLDOWN_MS = 60UL * 1000UL;
};

// ================= Safety Configuration =================
struct SafetyConfig {
    static constexpr size_t MIN_SAFE_HEAP = 8192;
    static constexpr unsigned long MAX_CONTINUOUS_MIX_MS = 2UL * 60UL * 1000UL;
    static constexpr unsigned long FAULT_WINDOW_MS = 5UL * 60UL * 1000UL;
    static constexpr unsigned int MAX_FAULTS_BEFORE_REBOOT = 3;
};

// ================= WiFi Reconnection Configuration =================
struct WiFiReconnectConfig {
    static constexpr unsigned long RETRY_INIT_MS = 30UL * 1000UL;
    static constexpr unsigned long RETRY_MAX_MS = 5UL * 60UL * 1000UL;
};

// ================= OTA Configuration =================
struct OTAConfig {
    static constexpr const char* HOSTNAME = "MilkMachine-V5";
    static constexpr const char* PASSWORD = "Gliders1!";
};

// ================= System States =================
enum class SystemState : uint8_t {
    IDLE,
    MIXING,
    POST_MIX,
    PERIODIC_MIX,
    WASH_STANDBY,
    FAULT
};

// ================= Event Types =================
enum class SystemEvent {
    LEVEL_SWITCH_LOW,
    LEVEL_SWITCH_HIGH,
    WASH_STANDBY_ACTIVATED,
    WASH_STANDBY_DEACTIVATED,
    WASH_DISPENSE_ACTIVATED,
    WASH_DISPENSE_DEACTIVATED,
    PERIODIC_MIX_TIMEOUT,
    POST_MIX_TIMEOUT,
    MIXING_TIMEOUT,
    FAULT_DETECTED,
    HEALTH_CHECK_FAILED,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED
};

#endif // CONFIG_H