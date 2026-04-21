#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "secrets.h"
#include "remote_monitor.h"
#include "monitor_config.h"
#include "machine_core.h"
#include "level_debouncer.h"

// ================= Hardware =================================
// Active-LOW relays
static constexpr int RELAY_AUGER    = 25;
static constexpr int RELAY_AGITATOR = 26;
static constexpr int RELAY_MIXER    = 16;
static constexpr int RELAY_WATER    = 17;
static constexpr int LEVEL_SWITCH   = 27;   // INPUT_PULLUP, LOW=active

// ================= Network ==================================
static const char* SSID     = SECRET_WIFI_SSID;
static const char* WPASS    = SECRET_WIFI_PASSWORD;
static const IPAddress LOCAL_IP(192, 168, 1, 16);
static const IPAddress GATEWAY (192, 168, 1,  1);
static const IPAddress SUBNET  (255, 255, 255, 0);
static const IPAddress DNS1    (  1,   1,   1, 1);
static const IPAddress DNS2    (  8,   8,   8, 8);

// ================= Machine ==================================
static Machine         machine;
static LevelDebouncer  levelDeb(50);

// ================= Timing / WDT =============================
static constexpr uint32_t WDT_TIMEOUT_S       = 10;
static constexpr uint32_t STATUS_INTERVAL_MS  = 10UL * 1000UL;
static constexpr uint32_t WIFI_RETRY_INIT_MS  = 30UL * 1000UL;
static constexpr uint32_t WIFI_RETRY_MAX_MS   =  5UL * 60UL * 1000UL;

static uint32_t wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
static uint32_t lastReconnectAttempt = 0;
static uint32_t lastStatusPrint      = 0;

// Relay output cache so we don't toggle GPIOs redundantly.
static bool cacheAuger = false, cacheAgit = false, cacheMix = false, cacheWater = false;

// ================= Forward decls ============================
static void initHardware();
static void initWiFi();
static void initOTA();
static void applyRelays(const MachineOutputs& out);
static void handleWiFiOTA(SysState state);
static void printStatus();

// ================= Hardware init ============================
// Drive pin HIGH before pinMode(OUTPUT) — on ESP32 the output latch is
// preserved, so the line is HIGH the instant the pin becomes an output.
// This avoids a LOW glitch on active-LOW relay coils.
static inline void presetOutputHigh(int pin) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
}

static void initHardware() {
    Serial.println(F("Initializing hardware..."));
    presetOutputHigh(RELAY_AUGER);
    presetOutputHigh(RELAY_AGITATOR);
    presetOutputHigh(RELAY_MIXER);
    presetOutputHigh(RELAY_WATER);

    pinMode(LEVEL_SWITCH, INPUT_PULLUP);
    bool lvl = (digitalRead(LEVEL_SWITCH) == LOW);  // true = milk needed
    levelDeb.reset(lvl, millis());
    Serial.println(F("Hardware initialized. Relays OFF."));
}

// Apply desired relay state with write-caching. Active-LOW.
static void applyRelays(const MachineOutputs& out) {
    if (out.auger    != cacheAuger)  { digitalWrite(RELAY_AUGER,    out.auger    ? LOW : HIGH); cacheAuger  = out.auger;    }
    if (out.agitator != cacheAgit)   { digitalWrite(RELAY_AGITATOR, out.agitator ? LOW : HIGH); cacheAgit   = out.agitator; }
    if (out.mixer    != cacheMix)    { digitalWrite(RELAY_MIXER,    out.mixer    ? LOW : HIGH); cacheMix    = out.mixer;    }
    if (out.water    != cacheWater)  { digitalWrite(RELAY_WATER,    out.water    ? LOW : HIGH); cacheWater  = out.water;    }
}

// ================= WiFi / OTA ===============================
static void initWiFi() {
    Serial.println(F("Initializing WiFi (non-blocking)..."));
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET, DNS1, DNS2)) {
        Serial.println(F("Static IP config failed."));
    }
    WiFi.begin(SSID, WPASS);
    lastReconnectAttempt = millis();
}

static void initOTA() {
    ArduinoOTA.setHostname("MilkMachine-V5");
    ArduinoOTA.setPassword(SECRET_OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
        // Force all relays off via hardware — bypass the machine for safety
        digitalWrite(RELAY_AUGER,    HIGH);
        digitalWrite(RELAY_AGITATOR, HIGH);
        digitalWrite(RELAY_MIXER,    HIGH);
        digitalWrite(RELAY_WATER,    HIGH);
        Serial.println(F("OTA start. Relays forced OFF."));
    });
    ArduinoOTA.onEnd([]() { Serial.println(F("OTA complete.")); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        esp_task_wdt_reset();
        unsigned pct = total ? (progress * 100U / total) : 0U;
        Serial.printf("OTA %u%%\r", pct);
    });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA Error[%u]\n", e); });
}

// Service WiFi, OTA, RemoteMonitor lifecycle.  Runs only on Core 1 (loop).
static void handleWiFiOTA(SysState state) {
    bool nowConnected = (WiFi.status() == WL_CONNECTED);
    static bool wasConnected = false;

    if (nowConnected && !wasConnected) {
        Serial.print(F("WiFi connected. IP: "));
        Serial.println(WiFi.localIP());
        ArduinoOTA.begin();
        if (!remoteMonitor.isEnabled()) {
            remoteMonitor.begin(MONITOR_TCP_PORT, MONITOR_WEB_PORT);
        }
    }
    if (!nowConnected && wasConnected) {
        Serial.println(F("WiFi disconnected."));
        remoteMonitor.end();
    }
    wasConnected = nowConnected;

    // OTA is safe in IDLE and FAULT — relays are OFF in both.
    if (nowConnected && (state == SysState::IDLE || state == SysState::FAULT)) {
        ArduinoOTA.handle();
    }

    // Reconnect backoff — exponential up to WIFI_RETRY_MAX_MS.
    if (!nowConnected) {
        uint32_t now = millis();
        if ((uint32_t)(now - lastReconnectAttempt) >= wifiRetryIntervalMs) {
            Serial.printf("WiFi retry (backoff %lu ms)...\n", (unsigned long)wifiRetryIntervalMs);
            WiFi.disconnect(false, false);
            WiFi.begin(SSID, WPASS);
            lastReconnectAttempt = now;
            wifiRetryIntervalMs  = min<uint32_t>(wifiRetryIntervalMs * 2UL, WIFI_RETRY_MAX_MS);
        }
    } else {
        wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
    }
}

// ================= Status ===================================
static void printStatus() {
    RLOG_PRINTLN("=== System Status ===");
    RLOG_PRINTF("State: %s\n", machine.stateStr());
    RLOG_PRINTF("Level: %s\n", levelDeb.stable() ? "LOW (milk needed)" : "HIGH (ok)");
    RLOG_PRINTF("WiFi: %s\n",  WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (remoteMonitor.isEnabled()) {
        RLOG_PRINTF("Remote Clients: %u\n", (unsigned)remoteMonitor.getClientCount());
    }
    RLOG_PRINTF("Faults (window): %u\n", (unsigned)machine.faultCount());
    RLOG_PRINTF("Uptime: %lu s\n", (unsigned long)(millis() / 1000UL));
    RLOG_PRINTF("Free Heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
    RLOG_PRINTLN("====================");
}

// ================= Arduino entry points =====================
void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println(F("\n=== Calf Feeding Machine V5 Hardened ==="));

    esp_reset_reason_t rr = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", (int)rr);

    initHardware();

    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    initWiFi();
    initOTA();

    machine.reset(millis());
    lastStatusPrint = millis();

    if (rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT || rr == ESP_RST_POWERON) {
        Serial.println(F("Safety cooldown on boot."));
        delay(500);
    }
    Serial.println(F("=== System Ready ==="));
}

void loop() {
    esp_task_wdt_reset();

    // --- Input: debounced level ---
    uint32_t now = millis();
    bool raw = (digitalRead(LEVEL_SWITCH) == LOW);  // LOW = milk needed
    if (levelDeb.update(raw, now)) {
        RLOG_PRINTF("Level stable: %s\n",
                    levelDeb.stable() ? "LOW (milk needed)" : "HIGH (ok)");
    }

    // --- State machine tick ---
    MachineInputs in{ levelDeb.stable(), now, ESP.getFreeHeap() };
    MachineOutputs outp;
    SysState prev = machine.state();
    machine.tick(in, outp);

    // Log transitions
    if (machine.state() != prev) {
        RLOG_PRINTF("STATE: %s (%s)\n", machine.stateStr(), machine.eventStr());
    }

    applyRelays(outp);

    if (outp.rebootRequested) {
        RLOG_PRINTLN("Too many faults in short time - performing controlled restart.");
        delay(100);
        esp_restart();
    }

    // --- Networking ---
    handleWiFiOTA(machine.state());
    remoteMonitor.handle();

    // --- Periodic status ---
    if ((uint32_t)(millis() - lastStatusPrint) >= STATUS_INTERVAL_MS) {
        printStatus();
        lastStatusPrint = millis();
    }
}
