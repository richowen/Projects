#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

// ================= Hardware Configuration =================
// Active LOW relays
const int RELAY_AUGER    = 25;
const int RELAY_AGITATOR = 26;
const int RELAY_MIXER    = 16;
const int RELAY_WATER    = 17;

// Move level switch off GPIO12 (strap pin). Use GPIO27 with internal pull-up.
const int LEVEL_SWITCH   = 27;   // Input pull-up, LOW = active

// ================= Network Configuration ==================
const char* ssid     = "WiFi";
const char* password = "Gliders1!";

IPAddress local_IP(192, 168, 1, 16);
IPAddress gateway (192, 168, 1, 1);
IPAddress subnet  (255, 255, 255, 0);
IPAddress dns1    (1, 1, 1, 1);
IPAddress dns2    (8, 8, 8, 8);

// ================== System States =========================
enum SystemState : uint8_t {
  IDLE,
  MIXING,
  POST_MIX,
  PERIODIC_MIX,
  FAULT
};

// ================== Globals ===============================
SystemState currentState = IDLE;

unsigned long lastStateChange     = 0;
unsigned long lastPeriodicMix     = 0;
unsigned long lastStatusPrint     = 0;
unsigned long lastReconnectAttempt= 0;
unsigned long faultEnteredAt      = 0;

// Debounce
unsigned long levelSwitchStableSince = 0;
int           levelSwitchStableState = HIGH;  // start high with pull-up
int           lastLevelRead          = HIGH;

// Relay cache (to avoid redundant writes)
bool relAuger=false, relAgitator=false, relMixer=false, relWater=false;

// ================ Reliability / Health ====================
// Minimum heap below which we consider the system unhealthy
const size_t MIN_SAFE_HEAP = 8192; // bytes

// Fault escalation: if we hit FAULT repeatedly within FAULT_WINDOW_MS, reboot
unsigned int faultCount = 0;
unsigned long lastFaultTimestamp = 0;
const unsigned long FAULT_WINDOW_MS = 5UL * 60UL * 1000UL; // 5 minutes
const unsigned int MAX_FAULTS_BEFORE_REBOOT = 3;

// Wi-Fi reconnection backoff (exponential, capped)
// define the initial retry interval constant before any variable uses
const unsigned long WIFI_RETRY_INIT_MS = 30UL * 1000;
// Keep the old symbol for compatibility so all references compile
const unsigned long WIFI_RETRY_INTERVAL = WIFI_RETRY_INIT_MS;
unsigned long wifiRetryIntervalMs = WIFI_RETRY_INTERVAL;
const unsigned long WIFI_RETRY_MAX_MS = 5UL * 60UL * 1000UL; // 5 minutes

// ================== Timing (ms) ===========================
const unsigned long POST_MIX_DURATION        = 5UL * 1000;
const unsigned long PERIODIC_MIX_DURATION    = 5UL * 1000;
const unsigned long PERIODIC_MIX_INTERVAL    = 5UL * 60UL * 1000; // 5 minutes
const unsigned long DEBOUNCE_STABLE_MS       = 50;
const unsigned long WDT_TIMEOUT_SECONDS      = 10;
const unsigned long STATUS_INTERVAL          = 10UL * 1000;

// Safety limits
const unsigned long MAX_CONTINUOUS_MIX_MS    = 2UL * 60UL * 1000; // 2 minutes max
const unsigned long FAULT_COOLDOWN_MS        = 60UL * 1000;       // 60 seconds

// ================== Forward Decls =========================
void initializeHardware();
void initializeWiFi();
void initializeOTA();
void applyRelayState(bool auger, bool agitator, bool mixer, bool water);
void allRelaysOff();
void mixingRelaysOn();
void mixerOnlyOn();
void serviceInputs();
void updateStateMachine();
void handleWiFiOTA();
void printSystemStatus(const char* reason = nullptr);
void enterFault(const char* why);
// Health monitor invoked regularly from loop()
void monitorHealth();
bool inStateFor(unsigned long ms) { return (millis() - lastStateChange) >= ms; }

// ================== Setup =================================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\n=== Calf Feeding Machine V5 Hardened ===");

  // Log reset reason
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", (int)rr);

  initializeHardware();

  // Watchdog
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true); // panic on WDT
  esp_task_wdt_add(NULL);

  // Networking - initialize state; OTA will be initialized on connect
  initializeWiFi();
  initializeOTA(); // configure OTA callbacks and hostname (but defer begin until connected)

  // Start timers
  lastPeriodicMix = millis();
  lastStatusPrint = millis();

  // If previous reset indicates instability, start with cooldown
  if (rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT || rr == ESP_RST_POWERON) {
    Serial.println("Safety cooldown on boot.");
    allRelaysOff();
    delay(500); // short, one-off delay at boot is acceptable
  }

  Serial.println("=== System Ready ===");
}

// ================== Main Loop =============================
void loop() {
  esp_task_wdt_reset();

  serviceInputs();
  updateStateMachine();

  // lightweight health checks and heap monitoring
  monitorHealth();

  handleWiFiOTA();

  if ((millis() - lastStatusPrint) >= STATUS_INTERVAL) {
    printSystemStatus();
    lastStatusPrint = millis();
  }
}

// ================== Hardware ==============================
void presetOutputHigh(int pin) { digitalWrite(pin, HIGH); pinMode(pin, OUTPUT); }

void initializeHardware() {
  Serial.println("Initializing hardware...");

  // Set outputs HIGH before OUTPUT to avoid glitches on active-low relays
  presetOutputHigh(RELAY_AUGER);
  presetOutputHigh(RELAY_AGITATOR);
  presetOutputHigh(RELAY_MIXER);
  presetOutputHigh(RELAY_WATER);

  // Inputs
  pinMode(LEVEL_SWITCH, INPUT_PULLUP);
  lastLevelRead = digitalRead(LEVEL_SWITCH);
  levelSwitchStableState = lastLevelRead;
  levelSwitchStableSince = millis();

  // Ensure safe state
  allRelaysOff();
  Serial.println("Hardware initialized. All relays OFF.");
}

// Centralized relay writer with caching
void applyRelayState(bool auger, bool agitator, bool mixer, bool water) {
  if (auger    != relAuger)    { digitalWrite(RELAY_AUGER,    auger ? LOW : HIGH);    relAuger = auger; }
  if (agitator != relAgitator) { digitalWrite(RELAY_AGITATOR, agitator ? LOW : HIGH); relAgitator = agitator; }
  if (mixer    != relMixer)    { digitalWrite(RELAY_MIXER,    mixer ? LOW : HIGH);    relMixer = mixer; }
  if (water    != relWater)    { digitalWrite(RELAY_WATER,    water ? LOW : HIGH);    relWater = water; }
}

void allRelaysOff()    { applyRelayState(false, false, false, false); }
void mixingRelaysOn()  { applyRelayState(true,  true,  true,  true ); }
void mixerOnlyOn()     { applyRelayState(false, false, true,  false); }

// ================== Wi-Fi / OTA ===========================
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("WiFi connected. IP: ");
      Serial.println(WiFi.localIP());
      // Initialize OTA service now that we have connectivity.
      // ArduinoOTA.begin() is safe to call multiple times, but we guard to avoid repeated init spam.
      Serial.println("Initializing OTA service (on WiFi connect)...");
      ArduinoOTA.begin();
      // Reset WiFi backoff interval on successful connect
      wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi disconnected.");
      break;
    default: break;
  }
}

void initializeWiFi() {
  Serial.println("Initializing WiFi (non-blocking)...");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.onEvent(onWiFiEvent);

  if (!WiFi.config(local_IP, gateway, subnet, dns1, dns2)) {
    Serial.println("Static IP config failed. Continuing.");
  }
  // start connect attempt
  WiFi.begin(ssid, password);
  lastReconnectAttempt = millis();
  wifiRetryIntervalMs = WIFI_RETRY_INIT_MS; // reset backoff on fresh start
}

void initializeOTA() {
  ArduinoOTA.setHostname("MilkMachine-V5");
  ArduinoOTA.setPassword("Gliders1!");

  ArduinoOTA.onStart([]() {
    allRelaysOff();
    Serial.println("OTA start. Relays forced OFF.");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA complete.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Keep watchdog happy during OTA
    esp_task_wdt_reset();
    unsigned int pct = total ? (progress * 100U / total) : 0U;
    Serial.printf("OTA %u%%\r", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]\n", error);
  });

  // Do not call begin() conditionally. It is safe if Wi-Fi is currently down.
  ArduinoOTA.begin();
  Serial.println("OTA initialized.");
}

// ================== Inputs ================================
void serviceInputs() {
  int reading = digitalRead(LEVEL_SWITCH);
  if (reading != lastLevelRead) {
    // reset stable timer on any change
    levelSwitchStableSince = millis();
    lastLevelRead = reading;
  } else {
    // if unchanged and stable long enough, latch it
    if ((millis() - levelSwitchStableSince) >= DEBOUNCE_STABLE_MS) {
      // only act if this is a new stable state
      if (reading != levelSwitchStableState) {
        levelSwitchStableState = reading;
        Serial.print("Level switch stable: ");
        Serial.println(levelSwitchStableState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");
      }
    }
  }
}

// ================== State Machine =========================
void transitionTo(SystemState s) {
  currentState = s;
  lastStateChange = millis();
}

void enterFault(const char* why) {
  allRelaysOff();
  transitionTo(FAULT);
  unsigned long now = millis();
  faultEnteredAt = now;
  Serial.print("FAULT: ");
  Serial.println(why);

  // Fault window handling
  if ((now - lastFaultTimestamp) <= FAULT_WINDOW_MS) {
    faultCount++;
  } else {
    faultCount = 1;
  }
  lastFaultTimestamp = now;

  Serial.printf("Fault count (window %lu ms): %u\n", FAULT_WINDOW_MS, faultCount);

  // Escalate to reboot if faults keep happening within the window
  if (faultCount >= MAX_FAULTS_BEFORE_REBOOT) {
    Serial.println("Too many faults in short time - performing controlled restart.");
    delay(100); // allow Serial to flush
    esp_restart();
  }
}

void updateStateMachine() {
  unsigned long now = millis();

  switch (currentState) {
    case IDLE:
      // Demand triggered
      if (levelSwitchStableState == LOW) {
        Serial.println("STATE: IDLE -> MIXING (level low)");
        mixingRelaysOn();
        transitionTo(MIXING);
        return;
      }
      // Periodic stir
      if ((now - lastPeriodicMix) >= PERIODIC_MIX_INTERVAL) {
        Serial.println("STATE: IDLE -> PERIODIC_MIX (interval elapsed)");
        mixerOnlyOn();
        transitionTo(PERIODIC_MIX);
        lastPeriodicMix = now;
        return;
      }
      break;

    case MIXING:
      // Safety cap
      if ((now - lastStateChange) >= MAX_CONTINUOUS_MIX_MS) {
        enterFault("Mixing exceeded max duration");
        return;
      }
      // Stop when level restored
      if (levelSwitchStableState == HIGH) {
        Serial.println("STATE: MIXING -> POST_MIX (level restored)");
        mixerOnlyOn();
        transitionTo(POST_MIX);
        return;
      }
      break;

    case POST_MIX:
      if (inStateFor(POST_MIX_DURATION)) {
        Serial.println("STATE: POST_MIX -> IDLE");
        allRelaysOff();
        transitionTo(IDLE);
        return;
      }
      break;

    case PERIODIC_MIX:
      if (inStateFor(PERIODIC_MIX_DURATION)) {
        Serial.println("STATE: PERIODIC_MIX -> IDLE");
        allRelaysOff();
        transitionTo(IDLE);
        return;
      }
      // If demand arises during periodic, escalate to full MIXING
      if (levelSwitchStableState == LOW) {
        Serial.println("STATE: PERIODIC_MIX -> MIXING (demand during periodic)");
        mixingRelaysOn();
        transitionTo(MIXING);
        return;
      }
      break;

    case FAULT:
      // Sit in FAULT with everything OFF, then auto-recover to IDLE after cooldown
      if ((now - faultEnteredAt) >= FAULT_COOLDOWN_MS) {
        Serial.println("STATE: FAULT -> IDLE (cooldown complete)");
        allRelaysOff();
        transitionTo(IDLE);
        // push out periodic mix timer so we do not immediately start again
        lastPeriodicMix = now;
      }
      break;
  }
}

// ================== Health Monitor ========================
void monitorHealth() {
  // Check heap
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < MIN_SAFE_HEAP) {
    Serial.printf("CRITICAL: low heap %u bytes < %u threshold\n", (unsigned)freeHeap, (unsigned)MIN_SAFE_HEAP);
    // attempt graceful fault then escalate via enterFault (which may reboot after repeated occurrences)
    enterFault("Low heap");
    return;
  }

  // Sanity: if we've been stuck in MIXING far longer than MAX_CONTINUOUS_MIX_MS (defensive guard)
  if (currentState == MIXING) {
    unsigned long now = millis();
    if ((now - lastStateChange) > (MAX_CONTINUOUS_MIX_MS + 10000UL)) { // extra 10s margin
      Serial.println("Sanity: MIXING exceeded safety+margin -> entering fault");
      enterFault("Mixing stuck beyond safety margin");
    }
  }
}

// ================== Wi-Fi + OTA Service ===================
void handleWiFiOTA() {
  // Service OTA only when safe
  if (currentState == IDLE && WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }

  // Reconnect strategy without blocking and with exponential backoff
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if ((now - lastReconnectAttempt) >= wifiRetryIntervalMs) {
      Serial.printf("WiFi retry (backoff %lu ms)...\n", wifiRetryIntervalMs);
      WiFi.disconnect(false, false);
      WiFi.begin(ssid, password);
      lastReconnectAttempt = now;
      // increase backoff for next attempt
      wifiRetryIntervalMs = wifiRetryIntervalMs * 2UL;
      if (wifiRetryIntervalMs > WIFI_RETRY_MAX_MS) wifiRetryIntervalMs = WIFI_RETRY_MAX_MS;
    }
  } else {
    // ensure backoff reset on successful connection
    wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
  }
}

// ================== Status ================================
void printSystemStatus(const char* reason) {
  Serial.println("=== System Status ===");
  Serial.print("State: ");
  switch (currentState) {
    case IDLE:          Serial.println("IDLE"); break;
    case MIXING:        Serial.println("MIXING"); break;
    case POST_MIX:      Serial.println("POST_MIX"); break;
    case PERIODIC_MIX:  Serial.println("PERIODIC_MIX"); break;
    case FAULT:         Serial.println("FAULT"); break;
  }

  Serial.print("Level Switch: ");
  Serial.println(levelSwitchStableState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");

  Serial.print("WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

  if (reason) {
    Serial.print("Note: ");
    Serial.println(reason);
  }

  Serial.printf("Uptime: %lu s\n", millis() / 1000UL);
  Serial.printf("Free Heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
  Serial.println("====================");
}
