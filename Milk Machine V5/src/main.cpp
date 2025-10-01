#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DFRobot_RGBLCD1602.h>
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

// Wash mode switches
const int WASH_STANDBY_PIN  = 23;   // Wash standby switch, LOW = active
const int WASH_DISPENSE_PIN = 5;    // Water solenoid activate switch in wash mode, LOW = active

// ================= Network Configuration ==================
const char* ssid     = "WiFi";
const char* password = "Gliders1!";

IPAddress local_IP(192, 168, 1, 16);
IPAddress gateway (192, 168, 1, 1);
IPAddress subnet  (255, 255, 255, 0);
IPAddress dns1    (1, 1, 1, 1);
IPAddress dns2    (8, 8, 8, 8);

// ================== MQTT Configuration ==================
const char* mqtt_server = "192.168.1.3";
const int mqtt_port = 1883;
const char* mqtt_user = "richowen";
const char* mqtt_pass = "p";
const char* mqtt_base_topic = "home/milk_machine";

// ================== Telnet Logging ======================
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// ================== MQTT ================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ================== LCD ================================
DFRobot_RGBLCD1602 lcd(16, 2);  // 16 columns, 2 rows

// A simple wrapper to print to both Serial and a Telnet client
class DualPrint : public Print {
private:
    WiFiClient* _client = nullptr;
public:
    void setClient(WiFiClient* client) {
        _client = client;
    }

    size_t write(uint8_t c) override {
        if (_client && _client->connected()) {
            _client->write(c);
        }
        return Serial.write(c);
    }
    size_t write(const uint8_t *buffer, size_t size) override {
        if (_client && _client->connected()) {
            _client->write(buffer, size);
        }
        return Serial.write(buffer, size);
    }
};

DualPrint Log;

// ================== System States =========================
enum SystemState : uint8_t {
  IDLE,
  MIXING,
  POST_MIX,
  PERIODIC_MIX,
  WASH_STANDBY,
  FAULT
};

// ================== Globals ===============================
SystemState currentState = IDLE;

unsigned long lastStateChange     = 0;
unsigned long lastPeriodicMix     = 0;
unsigned long lastStatusPrint     = 0;
unsigned long lastReconnectAttempt= 0;
unsigned long faultEnteredAt      = 0;
unsigned long lastMqttPublish     = 0;

// Debounce
unsigned long levelSwitchStableSince = 0;
int           levelSwitchStableState = HIGH;  // start high with pull-up
int           lastLevelRead          = HIGH;

unsigned long washStandbyStableSince = 0;
int           washStandbyStableState = HIGH;  // start high with pull-up
int           lastWashStandbyRead    = HIGH;

unsigned long washDispenseStableSince = 0;
int           washDispenseStableState = HIGH;  // start high with pull-up
int           lastWashDispenseRead    = HIGH;

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
const unsigned long MQTT_PUBLISH_INTERVAL    = 30UL * 1000; // 30 seconds

// Safety limits
const unsigned long MAX_CONTINUOUS_MIX_MS    = 2UL * 60UL * 1000; // 2 minutes max
const unsigned long FAULT_COOLDOWN_MS        = 60UL * 1000;       // 60 seconds

// ================== Forward Decls =========================
void initializeHardware();
void initializeWiFi();
void initializeOTA();
void initializeMQTT();
void initializeLCD();
void updateLCD();
void applyRelayState(bool auger, bool agitator, bool mixer, bool water);
void allRelaysOff();
void mixingRelaysOn();
void mixerOnlyOn();
void serviceInputs();
void updateStateMachine();
void handleWiFiOTA();
void handleTelnet();
void handleMQTT();
void publishSystemStatus();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void printSystemStatus(const char* reason = nullptr);
void enterFault(const char* why);
// Health monitor invoked regularly from loop()
void monitorHealth();
bool inStateFor(unsigned long ms) { return (millis() - lastStateChange) >= ms; }

// ================== Setup =================================
void setup() {
  Serial.begin(115200);
  delay(50);
  Log.println("\n=== Calf Feeding Machine V5 Hardened ===");

  // Log reset reason
  esp_reset_reason_t rr = esp_reset_reason();
  Log.printf("Reset reason: %d\n", (int)rr);

  initializeHardware();

  // Watchdog
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true); // panic on WDT
  esp_task_wdt_add(NULL);

  // Networking - initialize state; OTA will be initialized on connect
  initializeWiFi();
  initializeOTA(); // configure OTA callbacks and hostname (but defer begin until connected)
  initializeMQTT();
  initializeLCD();

  // Start timers
  lastPeriodicMix = millis();
  lastStatusPrint = millis();

  // If previous reset indicates instability, start with cooldown
  if (rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT || rr == ESP_RST_POWERON) {
    Log.println("Safety cooldown on boot.");
    allRelaysOff();
    delay(500); // short, one-off delay at boot is acceptable
  }

  Log.println("=== System Ready ===");
}

// ================== Main Loop =============================
void loop() {
  esp_task_wdt_reset();

  serviceInputs();
  updateStateMachine();
  
  // lightweight health checks and heap monitoring
  monitorHealth();
  
  handleWiFiOTA();
  handleTelnet();
  handleMQTT();

  if ((millis() - lastStatusPrint) >= STATUS_INTERVAL) {
    printSystemStatus();
    updateLCD();
    lastStatusPrint = millis();
  }
}

// ================== Hardware ==============================
void presetOutputHigh(int pin) { digitalWrite(pin, HIGH); pinMode(pin, OUTPUT); }

void initializeHardware() {
  Log.println("Initializing hardware...");

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
  
  pinMode(WASH_STANDBY_PIN, INPUT_PULLUP);
  lastWashStandbyRead = digitalRead(WASH_STANDBY_PIN);
  washStandbyStableState = lastWashStandbyRead;
  washStandbyStableSince = millis();
  
  pinMode(WASH_DISPENSE_PIN, INPUT_PULLUP);
  lastWashDispenseRead = digitalRead(WASH_DISPENSE_PIN);
  washDispenseStableState = lastWashDispenseRead;
  washDispenseStableSince = millis();

  // Ensure safe state
  allRelaysOff();
  Log.println("Hardware initialized. All relays OFF.");
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
      Log.print("WiFi connected. IP: ");
      Log.println(WiFi.localIP());
      Log.print("Gateway: ");
      Log.println(WiFi.gatewayIP());
      Log.print("Subnet: ");
      Log.println(WiFi.subnetMask());
      // Initialize OTA service now that we have connectivity.
      // ArduinoOTA.begin() is safe to call multiple times, but we guard to avoid repeated init spam.
      Log.println("Initializing OTA service (on WiFi connect)...");
      ArduinoOTA.begin();
      // Start Telnet server
      telnetServer.begin();
      Log.println("Telnet server started on port 23.");
      Log.printf("Connect via: telnet %s 23\n", WiFi.localIP().toString().c_str());
      // Reset WiFi backoff interval on successful connect
      wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
       Log.println("WiFi disconnected.");
       telnetServer.stop();
       Log.println("Telnet server stopped.");
       mqttClient.disconnect();
       Log.println("MQTT disconnected.");
       break;
    default: break;
  }
}

void initializeWiFi() {
  Log.println("Initializing WiFi (non-blocking)...");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.onEvent(onWiFiEvent);

  if (!WiFi.config(local_IP, gateway, subnet, dns1, dns2)) {
    Log.println("Static IP config failed. Continuing.");
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
    Log.println("OTA start. Relays forced OFF.");
  });
  ArduinoOTA.onEnd([]() {
    Log.println("OTA complete.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Keep watchdog happy during OTA
    esp_task_wdt_reset();
    unsigned int pct = total ? (progress * 100U / total) : 0U;
    Log.printf("OTA %u%%\r", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Log.printf("OTA Error[%u]\n", error);
  });

  ArduinoOTA.begin();
  Log.println("OTA initialized.");
  }
  
  void initializeMQTT() {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    Log.println("MQTT initialized.");
  }
  
  void initializeLCD() {
    lcd.init();
    lcd.setRGB(0, 255, 0);  // Green backlight
    lcd.print("Milk Machine V5");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    Log.println("LCD initialized.");
  }
  
  void updateLCD() {
    // Clear and set cursor
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("State: ");
  
    // Display current state
    switch (currentState) {
      case IDLE:          lcd.print("IDLE"); break;
      case MIXING:        lcd.print("MIXING"); break;
      case POST_MIX:      lcd.print("POST_MIX"); break;
      case PERIODIC_MIX:  lcd.print("PERIODIC"); break;
      case WASH_STANDBY:  lcd.print("WASH"); break;
      case FAULT:         lcd.print("FAULT"); break;
    }
  
    // Set backlight color based on state
    if (currentState == FAULT) {
      lcd.setRGB(255, 0, 0);  // Red for fault
    } else if (currentState == WASH_STANDBY) {
      lcd.setRGB(0, 0, 255);  // Blue for wash
    } else if (currentState == MIXING || currentState == PERIODIC_MIX) {
      lcd.setRGB(255, 165, 0);  // Orange for mixing
    } else {
      lcd.setRGB(0, 255, 0);  // Green for idle
    }
  }

// ================== Inputs ================================
void serviceInputs() {
  // Level switch
  int reading = digitalRead(LEVEL_SWITCH);
  if (reading != lastLevelRead) {
    levelSwitchStableSince = millis();
    lastLevelRead = reading;
  } else {
    if ((millis() - levelSwitchStableSince) >= DEBOUNCE_STABLE_MS) {
      if (reading != levelSwitchStableState) {
        levelSwitchStableState = reading;
        Log.print("Level switch stable: ");
        Log.println(levelSwitchStableState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");
      }
    }
  }

  // Wash standby switch
  reading = digitalRead(WASH_STANDBY_PIN);
  if (reading != lastWashStandbyRead) {
    washStandbyStableSince = millis();
    lastWashStandbyRead = reading;
  } else {
    if ((millis() - washStandbyStableSince) >= DEBOUNCE_STABLE_MS) {
      if (reading != washStandbyStableState) {
        washStandbyStableState = reading;
        Log.print("Wash standby switch stable: ");
        Log.println(washStandbyStableState == LOW ? "LOW (wash mode active)" : "HIGH (normal mode)");
      }
    }
  }

  // Wash dispense switch
  reading = digitalRead(WASH_DISPENSE_PIN);
  if (reading != lastWashDispenseRead) {
    washDispenseStableSince = millis();
    lastWashDispenseRead = reading;
  } else {
    if ((millis() - washDispenseStableSince) >= DEBOUNCE_STABLE_MS) {
      if (reading != washDispenseStableState) {
        washDispenseStableState = reading;
        Log.print("Wash dispense switch stable: ");
        Log.println(washDispenseStableState == LOW ? "LOW (dispensing water)" : "HIGH (water off)");
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
  Log.print("FAULT: ");
  Log.println(why);

  // Fault window handling
  if ((now - lastFaultTimestamp) <= FAULT_WINDOW_MS) {
    faultCount++;
  } else {
    faultCount = 1;
  }
  lastFaultTimestamp = now;

  Log.printf("Fault count (window %lu ms): %u\n", FAULT_WINDOW_MS, faultCount);

  // Escalate to reboot if faults keep happening within the window
  if (faultCount >= MAX_FAULTS_BEFORE_REBOOT) {
    Log.println("Too many faults in short time - performing controlled restart.");
    delay(100); // allow Log to flush
    esp_restart();
  }
}

void updateStateMachine() {
  unsigned long now = millis();

  switch (currentState) {
    case IDLE:
      // Wash mode takes priority
      if (washStandbyStableState == LOW) {
        Log.println("STATE: IDLE -> WASH_STANDBY (wash mode activated)");
        allRelaysOff();
        transitionTo(WASH_STANDBY);
        return;
      }
      // Demand triggered
      if (levelSwitchStableState == LOW) {
        Log.println("STATE: IDLE -> MIXING (level low)");
        mixingRelaysOn();
        transitionTo(MIXING);
        return;
      }
      // Periodic stir
      if ((now - lastPeriodicMix) >= PERIODIC_MIX_INTERVAL) {
        Log.println("STATE: IDLE -> PERIODIC_MIX (interval elapsed)");
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
        Log.println("STATE: MIXING -> POST_MIX (level restored)");
        mixerOnlyOn();
        transitionTo(POST_MIX);
        return;
      }
      break;

    case POST_MIX:
      if (inStateFor(POST_MIX_DURATION)) {
        Log.println("STATE: POST_MIX -> IDLE");
        allRelaysOff();
        transitionTo(IDLE);
        return;
      }
      break;

    case PERIODIC_MIX:
      if (inStateFor(PERIODIC_MIX_DURATION)) {
        Log.println("STATE: PERIODIC_MIX -> IDLE");
        allRelaysOff();
        transitionTo(IDLE);
        return;
      }
      // If demand arises during periodic, escalate to full MIXING
      if (levelSwitchStableState == LOW) {
        Log.println("STATE: PERIODIC_MIX -> MIXING (demand during periodic)");
        mixingRelaysOn();
        transitionTo(MIXING);
        return;
      }
      break;

    case WASH_STANDBY:
      // Control water solenoid based on dispense switch
      if (washDispenseStableState == LOW) {
        // Turn on water relay
        applyRelayState(false, false, false, true);  // only water
      } else {
        // Turn off water relay
        applyRelayState(false, false, false, false); // all off
      }
      // Exit wash mode when standby switch released
      if (washStandbyStableState == HIGH) {
        Log.println("STATE: WASH_STANDBY -> IDLE (wash mode deactivated)");
        allRelaysOff();
        transitionTo(IDLE);
        // Reset periodic timer to prevent immediate mixing
        lastPeriodicMix = now;
        return;
      }
      break;

    case FAULT:
      // Sit in FAULT with everything OFF, then auto-recover to IDLE after cooldown
      if ((now - faultEnteredAt) >= FAULT_COOLDOWN_MS) {
        Log.println("STATE: FAULT -> IDLE (cooldown complete)");
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
    Log.printf("CRITICAL: low heap %u bytes < %u threshold\n", (unsigned)freeHeap, (unsigned)MIN_SAFE_HEAP);
    // attempt graceful fault then escalate via enterFault (which may reboot after repeated occurrences)
    enterFault("Low heap");
    return;
  }

  // Sanity: if we've been stuck in MIXING far longer than MAX_CONTINUOUS_MIX_MS (defensive guard)
  if (currentState == MIXING) {
    unsigned long now = millis();
    if ((now - lastStateChange) > (MAX_CONTINUOUS_MIX_MS + 10000UL)) { // extra 10s margin
      Log.println("Sanity: MIXING exceeded safety+margin -> entering fault");
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
      Log.printf("WiFi retry (backoff %lu ms)...\n", wifiRetryIntervalMs);
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

// ================== Telnet Service =========================
void handleTelnet() {
    // Only handle telnet if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (telnetServer.hasClient()) {
        // If a new client connects, disconnect the old one
        if (telnetClient && telnetClient.connected()) {
            telnetClient.stop();
            Log.println("Telnet: New client connected, disconnecting old one.");
        }
        telnetClient = telnetServer.available();
        if (telnetClient) {
            Log.setClient(&telnetClient);
            Log.println("Telnet: Client connected from " + telnetClient.remoteIP().toString());
            // Send a welcome message and the current status
            telnetClient.println("\n=== Welcome to Milk Machine V5 Telnet Monitor ===");
            printSystemStatus("Telnet client connected");
        }
    }

        // Clean up disconnected clients
        if (telnetClient && !telnetClient.connected()) {
            // Note: Client may have already been stopped by a new connection
            // so we just nullify the handle.
            // telnetClient.stop(); // This is redundant if a new client took over
            Log.println("Telnet: Client disconnected.");
            Log.setClient(nullptr);
            // Setting telnetClient to a default-constructed client effectively clears it
            telnetClient = WiFiClient();
        }
    }
    
    // ================== Home Assistant Discovery ============
    void publishHADiscovery() {
        if (!mqttClient.connected()) return;
    
        // State sensor
        String configTopic = "homeassistant/sensor/milk_machine/state/config";
        String payload = "{";
        payload += "\"name\":\"Milk Machine State\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/state\",";
        payload += "\"unique_id\":\"milk_machine_state\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        // Level sensor
        configTopic = "homeassistant/binary_sensor/milk_machine/level/config";
        payload = "{";
        payload += "\"name\":\"Milk Level Low\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/level\",";
        payload += "\"payload_on\":\"LOW\",";
        payload += "\"payload_off\":\"HIGH\",";
        payload += "\"unique_id\":\"milk_machine_level\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        // Uptime sensor
        configTopic = "homeassistant/sensor/milk_machine/uptime/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Uptime\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/uptime\",";
        payload += "\"unit_of_measurement\":\"s\",";
        payload += "\"unique_id\":\"milk_machine_uptime\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        // Heap sensor
        configTopic = "homeassistant/sensor/milk_machine/heap/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Free Heap\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/heap\",";
        payload += "\"unit_of_measurement\":\"bytes\",";
        payload += "\"unique_id\":\"milk_machine_heap\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        // Fault count sensor
        configTopic = "homeassistant/sensor/milk_machine/fault_count/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Fault Count\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/fault_count\",";
        payload += "\"unique_id\":\"milk_machine_fault_count\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);

        // Wash standby sensor
        configTopic = "homeassistant/binary_sensor/milk_machine/wash_standby/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Wash Standby\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/wash_standby\",";
        payload += "\"payload_on\":\"ON\",";
        payload += "\"payload_off\":\"OFF\",";
        payload += "\"unique_id\":\"milk_machine_wash_standby\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);

        // Wash dispense sensor
        configTopic = "homeassistant/binary_sensor/milk_machine/wash_dispense/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Wash Dispense\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/wash_dispense\",";
        payload += "\"payload_on\":\"ON\",";
        payload += "\"payload_off\":\"OFF\",";
        payload += "\"unique_id\":\"milk_machine_wash_dispense\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        // Command switch
        configTopic = "homeassistant/switch/milk_machine/force_mix/config";
        payload = "{";
        payload += "\"name\":\"Milk Machine Force Mix\",";
        payload += "\"command_topic\":\"" + String(mqtt_base_topic) + "/command\",";
        payload += "\"payload_on\":\"force_mix\",";
        payload += "\"payload_off\":\"stop\",";
        payload += "\"state_topic\":\"" + String(mqtt_base_topic) + "/state\",";
        payload += "\"state_on\":\"MIXING\",";
        payload += "\"state_off\":\"IDLE\",";
        payload += "\"unique_id\":\"milk_machine_force_mix\",";
        payload += "\"device\":{\"identifiers\":[\"milk_machine_v5\"],\"name\":\"Milk Machine V5\",\"model\":\"ESP32 Milk Mixer\",\"manufacturer\":\"Custom\"}";
        payload += "}";
        mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    
        Log.println("MQTT: HA Discovery published");
    }
    
    // ================== MQTT Publish =========================
    void publishSystemStatus() {
        if (!mqttClient.connected()) return;
    
        char buffer[32];
    
        // State
        const char* stateStr;
        switch (currentState) {
            case IDLE: stateStr = "IDLE"; break;
            case MIXING: stateStr = "MIXING"; break;
            case POST_MIX: stateStr = "POST_MIX"; break;
            case PERIODIC_MIX: stateStr = "PERIODIC_MIX"; break;
            case WASH_STANDBY: stateStr = "WASH_STANDBY"; break;
            case FAULT: stateStr = "FAULT"; break;
        }
        mqttClient.publish((String(mqtt_base_topic) + "/state").c_str(), stateStr);
    
        // Level switch
        mqttClient.publish((String(mqtt_base_topic) + "/level").c_str(), levelSwitchStableState == LOW ? "LOW" : "HIGH");

        // Wash switches
        mqttClient.publish((String(mqtt_base_topic) + "/wash_standby").c_str(), washStandbyStableState == LOW ? "ON" : "OFF");
        mqttClient.publish((String(mqtt_base_topic) + "/wash_dispense").c_str(), washDispenseStableState == LOW ? "ON" : "OFF");
    
        // Uptime
        sprintf(buffer, "%lu", millis() / 1000UL);
        mqttClient.publish((String(mqtt_base_topic) + "/uptime").c_str(), buffer);
    
        // Free heap
        sprintf(buffer, "%u", (unsigned)ESP.getFreeHeap());
        mqttClient.publish((String(mqtt_base_topic) + "/heap").c_str(), buffer);
    
        // Fault count
        sprintf(buffer, "%u", faultCount);
        mqttClient.publish((String(mqtt_base_topic) + "/fault_count").c_str(), buffer);
    
        Log.println("MQTT: Status published");
    }
    
    // ================== MQTT Callback =========================
    void mqttCallback(char* topic, byte* payload, unsigned int length) {
        String message;
        for (unsigned int i = 0; i < length; i++) {
            message += (char)payload[i];
        }
    
        Log.printf("MQTT: Received %s: %s\n", topic, message.c_str());
    
        if (String(topic) == String(mqtt_base_topic) + "/command") {
            if (message == "force_mix" && currentState == IDLE) {
                Log.println("MQTT: Force mixing command received");
                mixingRelaysOn();
                transitionTo(MIXING);
            } else if (message == "stop" && (currentState == MIXING || currentState == PERIODIC_MIX)) {
                Log.println("MQTT: Stop command received");
                allRelaysOff();
                transitionTo(IDLE);
            }
        }
    }
    
    // ================== MQTT Service =========================
    void handleMQTT() {
        if (WiFi.status() != WL_CONNECTED) {
            return;
        }
    
        if (!mqttClient.connected()) {
            Log.println("MQTT: Attempting connection...");
            if (mqttClient.connect("MilkMachineV5", mqtt_user, mqtt_pass)) {
                Log.println("MQTT: Connected");
                // Publish HA discovery
                publishHADiscovery();
                // Subscribe to command topic
                mqttClient.subscribe((String(mqtt_base_topic) + "/command").c_str());
                // Publish initial status
                publishSystemStatus();
            } else {
                Log.printf("MQTT: Failed to connect, rc=%d\n", mqttClient.state());
            }
        }
    
        mqttClient.loop();
    
        // Publish status periodically
        if ((millis() - lastMqttPublish) >= MQTT_PUBLISH_INTERVAL) {
            publishSystemStatus();
            lastMqttPublish = millis();
        }
    }


// ================== Status ================================
void printSystemStatus(const char* reason) {
  Log.println("=== System Status ===");
  Log.print("State: ");
  switch (currentState) {
    case IDLE:          Log.println("IDLE"); break;
    case MIXING:        Log.println("MIXING"); break;
    case POST_MIX:      Log.println("POST_MIX"); break;
    case PERIODIC_MIX:  Log.println("PERIODIC_MIX"); break;
    case WASH_STANDBY:  Log.println("WASH_STANDBY"); break;
    case FAULT:         Log.println("FAULT"); break;
  }

  Log.print("Level Switch: ");
  Log.println(levelSwitchStableState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");

  Log.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    Log.print("Connected (IP: ");
    Log.print(WiFi.localIP());
    Log.println(")");
    Log.print("Telnet: ");
    Log.println(telnetClient.connected() ? "Client connected" : "No client");
  } else {
    Log.println("Disconnected");
  }

  if (reason) {
    Log.print("Note: ");
    Log.println(reason);
  }

  Log.printf("Uptime: %lu s\n", millis() / 1000UL);
  Log.printf("Free Heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
  Log.println("====================");
}
