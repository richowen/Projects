#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

// Hardware Configuration - Pin Assignments
const int RELAY_AUGER = 25;      // Active LOW relay
const int RELAY_AGITATOR = 26;   // Active LOW relay  
const int RELAY_MIXER = 16;      // Active LOW relay
const int RELAY_WATER = 17;      // Active LOW relay
const int LEVEL_SWITCH = 12;     // Input pull-up, LOW=active

// Network Configuration
const char* ssid = "WiFi";
const char* password = "Gliders1!";
IPAddress local_IP(192, 168, 1, 16);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// System States
enum SystemState {
  IDLE,
  MIXING,
  POST_MIX,
  PERIODIC_MIX
};

// Global Variables
SystemState currentState = IDLE;
unsigned long lastStateChange = 0;
unsigned long lastPeriodicMix = 0;
unsigned long levelSwitchDebounceTime = 0;
bool lastLevelSwitchState = HIGH;
bool levelSwitchState = HIGH;

// Timing Constants (milliseconds)
const unsigned long POST_MIX_DURATION = 5000;        // 5 seconds
const unsigned long PERIODIC_MIX_DURATION = 5000;    // 5 seconds  
const unsigned long PERIODIC_MIX_INTERVAL = 300000;  // 5 minutes
const unsigned long DEBOUNCE_DELAY = 50;             // 50ms debounce
const unsigned long WDT_TIMEOUT = 10;                // 10 second watchdog

// Function Declarations
void initializeHardware();
void initializeWiFi();
void initializeOTA();
void allRelaysOff();
void mixingRelaysOn();
void mixerRelayOn();
void readLevelSwitch();
void updateStateMachine();
void handleWiFiOTA();
void printSystemStatus();

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Calf Feeding Machine V5 Starting ===");
  
  // Initialize hardware first (critical for safety)
  initializeHardware();
  
  // Initialize watchdog timer for reliability
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  
  // Initialize WiFi and OTA (non-critical, can fail)
  initializeWiFi();
  initializeOTA();
  
  Serial.println("=== System Ready - Core Mixing Operation Active ===");
  lastPeriodicMix = millis();
}

void loop() {
  // Feed watchdog timer (critical for reliability)
  esp_task_wdt_reset();
  
  // Core operations (highest priority)
  readLevelSwitch();
  updateStateMachine();
  
  // Non-critical operations (lowest priority)
  handleWiFiOTA();
  
  // Status reporting
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 10000) { // Every 10 seconds
    printSystemStatus();
    lastStatusPrint = millis();
  }
}

void initializeHardware() {
  Serial.println("Initializing hardware...");
  
  // Configure relay pins as outputs and ensure OFF state (safety first)
  pinMode(RELAY_AUGER, OUTPUT);
  pinMode(RELAY_AGITATOR, OUTPUT);
  pinMode(RELAY_MIXER, OUTPUT);
  pinMode(RELAY_WATER, OUTPUT);
  
  // Ensure all relays are OFF on startup (active LOW = HIGH output)
  allRelaysOff();
  
  // Configure level switch with internal pull-up
  pinMode(LEVEL_SWITCH, INPUT_PULLUP);
  
  // Read initial level switch state
  levelSwitchState = digitalRead(LEVEL_SWITCH);
  lastLevelSwitchState = levelSwitchState;
  
  Serial.println("Hardware initialized - All relays OFF (safe state)");
}

void initializeWiFi() {
  Serial.println("Initializing WiFi...");
  
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("WiFi static IP configuration failed");
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  
  // Wait for connection with timeout (don't block core operation)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connected - IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed - Core operation continues");
  }
}

void initializeOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA disabled - WiFi not connected");
    return;
  }
  
  Serial.println("Initializing OTA...");
  
  ArduinoOTA.setHostname("MilkMachine-V5");
  ArduinoOTA.setPassword("Gliders1!");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    
    // Stop all relays during OTA for safety
    allRelaysOff();
    Serial.println("OTA Update Starting: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA initialized and ready");
}

void allRelaysOff() {
  // Active LOW relays - HIGH = OFF
  digitalWrite(RELAY_AUGER, HIGH);
  digitalWrite(RELAY_AGITATOR, HIGH);
  digitalWrite(RELAY_MIXER, HIGH);
  digitalWrite(RELAY_WATER, HIGH);
}

void mixingRelaysOn() {
  // Active LOW relays - LOW = ON
  digitalWrite(RELAY_AUGER, LOW);
  digitalWrite(RELAY_AGITATOR, LOW);
  digitalWrite(RELAY_MIXER, LOW);
  digitalWrite(RELAY_WATER, LOW);
}

void mixerRelayOn() {
  // Turn off all relays first, then turn on only mixer
  allRelaysOff();
  digitalWrite(RELAY_MIXER, LOW);
}

void readLevelSwitch() {
  // Read current switch state
  int reading = digitalRead(LEVEL_SWITCH);
  
  // Debounce logic
  if (reading != lastLevelSwitchState) {
    levelSwitchDebounceTime = millis();
  }
  
  if ((millis() - levelSwitchDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != levelSwitchState) {
      levelSwitchState = reading;
      Serial.print("Level switch changed: ");
      Serial.println(levelSwitchState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");
    }
  }
  
  lastLevelSwitchState = reading;
}

void updateStateMachine() {
  unsigned long currentTime = millis();
  
  switch (currentState) {
    case IDLE:
      // Check for level switch trigger (LOW = milk needed)
      if (levelSwitchState == LOW) {
        Serial.println("STATE: IDLE -> MIXING (level switch active)");
        currentState = MIXING;
        mixingRelaysOn();
        lastStateChange = currentTime;
      }
      // Check for periodic mixing trigger
      else if (currentTime - lastPeriodicMix >= PERIODIC_MIX_INTERVAL) {
        Serial.println("STATE: IDLE -> PERIODIC_MIX (5 minute timer)");
        currentState = PERIODIC_MIX;
        mixerRelayOn();
        lastStateChange = currentTime;
        lastPeriodicMix = currentTime;
      }
      break;
      
    case MIXING:
      // Check if level switch is now HIGH (milk sufficient)
      if (levelSwitchState == HIGH) {
        Serial.println("STATE: MIXING -> POST_MIX (level switch restored)");
        currentState = POST_MIX;
        mixerRelayOn();  // Only mixer on for post-mix
        lastStateChange = currentTime;
      }
      break;
      
    case POST_MIX:
      // Check if post-mix duration completed
      if (currentTime - lastStateChange >= POST_MIX_DURATION) {
        Serial.println("STATE: POST_MIX -> IDLE (5 second timer completed)");
        currentState = IDLE;
        allRelaysOff();
        lastStateChange = currentTime;
      }
      break;
      
    case PERIODIC_MIX:
      // Check if periodic mix duration completed
      if (currentTime - lastStateChange >= PERIODIC_MIX_DURATION) {
        Serial.println("STATE: PERIODIC_MIX -> IDLE (5 second timer completed)");
        currentState = IDLE;
        allRelaysOff();
        lastStateChange = currentTime;
      }
      break;
  }
}

void handleWiFiOTA() {
  // Only handle OTA if WiFi is connected and system is in IDLE state
  if (WiFi.status() == WL_CONNECTED && currentState == IDLE) {
    ArduinoOTA.handle();
  }
  
  // Reconnect WiFi if disconnected (non-blocking)
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 30000) { // Try every 30 seconds
      Serial.println("WiFi disconnected - attempting reconnection...");
      WiFi.begin(ssid, password);
      lastReconnectAttempt = millis();
    }
  }
}

void printSystemStatus() {
  Serial.println("=== System Status ===");
  Serial.print("State: ");
  
  switch (currentState) {
    case IDLE:
      Serial.println("IDLE");
      break;
    case MIXING:
      Serial.println("MIXING");
      break;
    case POST_MIX:
      Serial.println("POST_MIX");
      break;
    case PERIODIC_MIX:
      Serial.println("PERIODIC_MIX");
      break;
  }
  
  Serial.print("Level Switch: ");
  Serial.println(levelSwitchState == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)");
  
  Serial.print("WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  
  Serial.println("====================");
}