#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "DFRobot_RGBLCD1602.h"

// WiFi credentials
const char* ssid = "WiFi";
const char* password = "Gliders1!";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 16);      // Static IP for ESP32
IPAddress gateway(192, 168, 1, 1);       // Router gateway
IPAddress subnet(255, 255, 255, 0);      // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);        // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);      // Google DNS backup

// MQTT Broker settings
const char* mqtt_server = "192.168.1.3";
const int mqtt_port = 1883;
const char* mqtt_user = "richowen";
const char* mqtt_password = "p";

// MQTT Topics
const char* status_topic = "milk_mixer/status";
const char* command_topic = "milk_mixer/command";
const char* error_topic = "milk_mixer/error";
const char* availability_topic = "milk_mixer/available";

// Pin Definitions
const int RELAY_AUGER = 25;      // Powder auger relay
const int RELAY_AGITATOR = 26;   // Powder agitator relay
const int RELAY_MIXER = 16;      // Liquid mixer relay
const int RELAY_WATER = 17;      // Water solenoid relay
const int LEVEL_SWITCH = 12;     // Pressure switch input
#define WASH_STANDBY_PIN 23      // Wash standby switch
#define WASH_DISPENSE_PIN 5      // Water solenoid activate switch in wash mode

// Constants
const unsigned long MIXING_TIMEOUT = 60000; // 1 minutes max mixing time
const unsigned long POST_MIX_TIME = 5000;   // 5 seconds post-mix time
const unsigned long ERROR_RETRY_DELAY = 300000; // 5 minutes between retries
const unsigned long WATCHDOG_INTERVAL = 100;  // Check system every 100ms
const unsigned long LCD_UPDATE_INTERVAL = 500; // Update LCD every 500ms
const unsigned long LEVEL_DEBOUNCE_TIME = 1000; // 1 second debounce for level switch
const unsigned long MQTT_RECONNECT_TIMEOUT = 10000; // 10 second MQTT timeout
const unsigned long WASH_TIMEOUT = 30000; // 30 second max water on time

// System States
enum SystemState {
    IDLE,
    MIXING,
    POST_MIXING,
    ERROR,
    WASH
};

// Error Types
enum ErrorType {
    NO_ERROR,
    TIMEOUT_ERROR,
    LEVEL_SWITCH_ERROR
};

// Global Variables
SystemState currentState = IDLE;
ErrorType currentError = NO_ERROR;
unsigned long mixingStartTime = 0;
unsigned long postMixStartTime = 0;
unsigned long errorStartTime = 0;
unsigned long lastWatchdogCheck = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long levelSwitchDebounceStart = 0;
unsigned long washWaterStartTime = 0;
bool levelSwitchLastState = false;
bool levelSwitchStable = false;
bool washWaterActive = false;
String lastLCDLine1 = "";
String lastLCDLine2 = "";

// Objects
WiFiClient espClient;
PubSubClient mqtt(espClient);
DFRobot_RGBLCD1602 lcd(/*RGBAddr*/0x2D, /*lcdCols*/16, /*lcdRows*/2);

// Function Declarations
void setupWiFi();
void reconnectMQTT();
void handleMQTTMessage(char* topic, byte* payload, unsigned int length);
void reportStatus();
void startMixing();
void stopMixing();
void handleError(ErrorType error);
void watchdogCheck();
void updateLCD();
bool timeElapsed(unsigned long start, unsigned long interval);

void setup() {
    Serial.begin(115200);
    
    // Initialize LCD
    lcd.init();
    lcd.setRGB(0, 0, 255); // Blue for startup
    lcd.clear();
    lcd.print("Milk Mixer v1.0");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    
    // Initialize pins
    pinMode(RELAY_AUGER, OUTPUT);
    pinMode(RELAY_AGITATOR, OUTPUT);
    pinMode(RELAY_MIXER, OUTPUT);
    pinMode(RELAY_WATER, OUTPUT);
    pinMode(LEVEL_SWITCH, INPUT_PULLUP);
    pinMode(WASH_STANDBY_PIN, INPUT_PULLUP);
    pinMode(WASH_DISPENSE_PIN, INPUT_PULLUP);
    
    // Ensure all relays are off initially (relays are active LOW)
    digitalWrite(RELAY_AUGER, HIGH);
    digitalWrite(RELAY_AGITATOR, HIGH);
    digitalWrite(RELAY_MIXER, HIGH);
    digitalWrite(RELAY_WATER, HIGH);
    
    // Setup WiFi and MQTT
    setupWiFi();
    
    // Setup OTA updates (only if WiFi connected)
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.setHostname("MilkMixer-OTA");
        
        ArduinoOTA.onStart([]() {
            // Block OTA during active operations
            if (currentState == MIXING || currentState == POST_MIXING) {
                lcd.clear();
                lcd.setRGB(255, 0, 0);
                lcd.print("OTA Blocked");
                lcd.setCursor(0, 1);
                lcd.print("System Active");
                return;
            }
            
            lcd.clear();
            lcd.setRGB(255, 255, 0); // Yellow for OTA
            lcd.print("OTA Update...");
            Serial.println("OTA Update Starting");
        });
        
        ArduinoOTA.onEnd([]() {
            lcd.setCursor(0, 1);
            lcd.print("Complete!");
            Serial.println("OTA Update Complete");
        });
        
        ArduinoOTA.onError([](ota_error_t error) {
            lcd.clear();
            lcd.setRGB(255, 0, 0);
            lcd.print("OTA Error!");
            Serial.printf("OTA Error[%u]: ", error);
        });
        
        ArduinoOTA.begin();
        Serial.println("OTA Ready");
    }
    
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(handleMQTTMessage);
    
    // Report initial status (only if MQTT is connected)
    if (mqtt.connected()) {
        mqtt.publish(availability_topic, "online", true);
        reportStatus();
    }
    
    // Initialize level switch state for reliable operation
    levelSwitchLastState = digitalRead(LEVEL_SWITCH) == LOW;
    levelSwitchStable = true;
    levelSwitchDebounceStart = 0;
    
    // Update LCD after initialization
    lcd.clear();
    updateLCD();
}

void loop() {
    // Check wash mode switch first
    if (digitalRead(WASH_STANDBY_PIN) == LOW) {
        if (currentState != WASH) {
            currentState = WASH;
            // Ensure all relays except water are off
            digitalWrite(RELAY_AUGER, HIGH);
            digitalWrite(RELAY_AGITATOR, HIGH);
            digitalWrite(RELAY_MIXER, HIGH);
            washWaterActive = false;
            reportStatus();
        }
        
        // Control water relay based on dispense switch with safety timeout
        bool shouldActivateWater = digitalRead(WASH_DISPENSE_PIN) == LOW;
        
        if (shouldActivateWater && !washWaterActive) {
            // Start water
            digitalWrite(RELAY_WATER, LOW);
            washWaterActive = true;
            washWaterStartTime = millis();
        } else if (!shouldActivateWater && washWaterActive) {
            // Stop water
            digitalWrite(RELAY_WATER, HIGH);
            washWaterActive = false;
        } else if (washWaterActive && timeElapsed(washWaterStartTime, WASH_TIMEOUT)) {
            // Safety timeout - force water off
            digitalWrite(RELAY_WATER, HIGH);
            washWaterActive = false;
            Serial.println("Wash mode safety timeout - water forced off");
        }
    } else if (currentState == WASH) {
        // Exit wash mode
        currentState = IDLE;
        digitalWrite(RELAY_WATER, HIGH);
        washWaterActive = false;
        reportStatus();
    }

    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();
    
    // Handle OTA updates
    ArduinoOTA.handle();

    // LCD updates
    if (timeElapsed(lastLCDUpdate, LCD_UPDATE_INTERVAL)) {
        updateLCD();
        lastLCDUpdate = millis();
    }

    // Watchdog checks
    if (timeElapsed(lastWatchdogCheck, WATCHDOG_INTERVAL)) {
        watchdogCheck();
        lastWatchdogCheck = millis();
    }

    // Main state machine
    switch (currentState) {
        case WASH:
            // Handled at start of loop
            break;
            
        case IDLE: {
            // Automatic start when level switch activates (water level drops)
            bool currentLevelState = digitalRead(LEVEL_SWITCH) == LOW;
            if (currentLevelState != levelSwitchLastState) {
                levelSwitchDebounceStart = millis();
                levelSwitchStable = false;
                levelSwitchLastState = currentLevelState;
            } else if (!levelSwitchStable && timeElapsed(levelSwitchDebounceStart, LEVEL_DEBOUNCE_TIME)) {
                levelSwitchStable = true;
                if (currentLevelState) { // Level switch activated (water level low) and stable
                    Serial.println("Level switch activated - starting automatic mixing cycle");
                    startMixing();
                }
            }
            break;
        }
            
        case MIXING: {
            if (timeElapsed(mixingStartTime, MIXING_TIMEOUT)) {
                handleError(TIMEOUT_ERROR);
                break;
            }
            
            // Level switch debouncing with time-based approach
            bool currentLevelState = digitalRead(LEVEL_SWITCH) == LOW;
            if (currentLevelState != levelSwitchLastState) {
                levelSwitchDebounceStart = millis();
                levelSwitchStable = false;
                levelSwitchLastState = currentLevelState;
            } else if (!levelSwitchStable && timeElapsed(levelSwitchDebounceStart, LEVEL_DEBOUNCE_TIME)) {
                levelSwitchStable = true;
                if (!currentLevelState) { // Level switch released (water level restored) and stable
                    Serial.println("Water level restored - moving to post-mixing");
                    currentState = POST_MIXING;
                    postMixStartTime = millis();
                    
                    // Turn off all except mixer
                    digitalWrite(RELAY_AUGER, HIGH);
                    digitalWrite(RELAY_AGITATOR, HIGH);
                    digitalWrite(RELAY_WATER, HIGH);
                    
                    reportStatus();
                }
            }
            break;
        }
            
        case POST_MIXING:
            if (timeElapsed(postMixStartTime, POST_MIX_TIME)) {
                stopMixing();
                currentState = IDLE;
                reportStatus();
            }
            break;
            
        case ERROR:
            if (timeElapsed(errorStartTime, ERROR_RETRY_DELAY)) {
                currentState = IDLE;
                currentError = NO_ERROR;
                reportStatus();
            }
            break;
    }
}

void updateLCD() {
    String line1, line2;
    static SystemState lastState = IDLE;
    
    // Update LCD color only when state changes
    if (currentState != lastState) {
        switch (currentState) {
            case IDLE:
                lcd.setRGB(0, 0, 255); // Blue
                break;
            case MIXING:
            case POST_MIXING:
                lcd.setRGB(0, 255, 0); // Green
                break;
            case ERROR:
                lcd.setRGB(255, 0, 0); // Red
                break;
            case WASH:
                lcd.setRGB(0, 255, 255); // Cyan
                break;
        }
        lastState = currentState;
    }
    
    // Build display content
    switch (currentState) {
        case IDLE:
            line1 = "Status: IDLE";
            line2 = "Ready to Start";
            break;
        case MIXING:
            line1 = "Status: MIXING";
            line2 = "Running...";
            break;
        case POST_MIXING: {
            int remainingTime = (POST_MIX_TIME - (millis() - postMixStartTime)) / 1000;
            line1 = "Post-Mix: " + String(remainingTime) + "s";
            line2 = "Finishing...";
            break;
        }
        case ERROR:
            line1 = "ERROR";
            switch (currentError) {
                case TIMEOUT_ERROR:
                    line2 = "Timeout Error!";
                    break;
                case LEVEL_SWITCH_ERROR:
                    line2 = "Level Sw Error!";
                    break;
                default:
                    line2 = "Unknown Error!";
            }
            break;
        case WASH:
            line1 = "WASH MODE";
            line2 = digitalRead(WASH_DISPENSE_PIN) == LOW ? "Water: ON" : "Water: OFF";
            break;
    }
    
    // Only update if content changed
    if (line1 != lastLCDLine1) {
        lcd.setCursor(0, 0);
        lcd.print("                "); // Clear line
        lcd.setCursor(0, 0);
        lcd.print(line1);
        lastLCDLine1 = line1;
    }
    
    if (line2 != lastLCDLine2) {
        lcd.setCursor(0, 1);
        lcd.print("                "); // Clear line
        lcd.setCursor(0, 1);
        lcd.print(line2);
        lastLCDLine2 = line2;
    }
}

void setupWiFi() {
    lcd.clear();
    lcd.print("Connecting WiFi");

    delay(10);
    Serial.println("Clearing WiFi config...");
    
    // Clear any cached WiFi configuration
    WiFi.disconnect(true);  // Disconnect and erase stored WiFi config
    WiFi.mode(WIFI_OFF);    // Turn off WiFi
    delay(1000);
    WiFi.mode(WIFI_STA);    // Set to station mode
    
    Serial.println("Scanning for networks...");
    lcd.setCursor(0, 1);
    lcd.print("Scanning...");
    
    // Scan for networks
    int n = WiFi.scanNetworks();
    Serial.printf("Found %d networks\n", n);
    
    int bestRSSI = -100;
    int bestChannel = 0;
    String bestBSSID = "";
    
    // Find the strongest network with our SSID
    for (int i = 0; i < n; i++) {
        String foundSSID = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        String bssid = WiFi.BSSIDstr(i);
        int channel = WiFi.channel(i);
        
        Serial.printf("Network %d: %s (RSSI: %d, Channel: %d, BSSID: %s)\n", 
                     i, foundSSID.c_str(), rssi, channel, bssid.c_str());
        
        if (foundSSID == ssid && rssi > bestRSSI) {
            bestRSSI = rssi;
            bestChannel = channel;
            bestBSSID = bssid;
        }
    }
    
    if (bestRSSI > -100) {
        Serial.printf("Best network: RSSI %d, Channel %d, BSSID %s\n", 
                     bestRSSI, bestChannel, bestBSSID.c_str());
        lcd.setCursor(0, 1);
        lcd.print("Best: " + String(bestRSSI) + "dBm");
        delay(1000);
    } else {
        Serial.println("Target network not found!");
        lcd.setCursor(0, 1);
        lcd.print("Network not found");
        delay(2000);
        return;
    }
    
    Serial.println("Configuring static IP...");
    
    // Configure static IP
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Static IP configuration failed");
        lcd.setCursor(0, 1);
        lcd.print("IP Config Failed");
        delay(2000);
    }
    
    Serial.println("Connecting to best WiFi...");
    // Connect to specific BSSID for best signal
    uint8_t bssid[6];
    sscanf(bestBSSID.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
           &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
    WiFi.begin(ssid, password, bestChannel, bssid);

    int dots = 0;
    unsigned long startAttemptTime = millis();
    const unsigned long wifiTimeout = 10000; // 10 seconds

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
        delay(500);
        Serial.print(".");
        lcd.setCursor(dots, 1);
        lcd.print(".");
        dots = (dots + 1) % 16;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());

        lcd.clear();
        lcd.print("WiFi Connected");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed");
        lcd.clear();
        lcd.print("WiFi Failed");
    }

    delay(2000);
}

void reconnectMQTT() {
    if (!timeElapsed(lastMQTTAttempt, MQTT_RECONNECT_TIMEOUT)) return;
    
    Serial.println("Attempting MQTT connection...");
    String clientId = "MilkMixer-";
    clientId += String(random(0xffff), HEX);
    
    // Connect with Last Will Testament - if device disconnects unexpectedly, broker will publish "offline"
    if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_password, availability_topic, 1, true, "offline")) {
        Serial.println("MQTT connected");
        mqtt.publish(availability_topic, "online", true);
        reportStatus();
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqtt.state());
    }
    lastMQTTAttempt = millis();
}

void handleMQTTMessage(char* topic, byte* payload, unsigned int length) {
    // MQTT is monitoring only - no commands processed
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("MQTT message received (monitoring only): " + message);
}

void reportStatus() {
    if (!mqtt.connected()) return; // Only publish if MQTT is connected
    
    String status;
    switch (currentState) {
        case IDLE:
            status = "idle";
            break;
        case MIXING:
            status = "mixing";
            break;
        case POST_MIXING:
            status = "post_mixing";
            break;
        case WASH:
            status = "wash_mode";
            break;
        case ERROR:
            switch (currentError) {
                case TIMEOUT_ERROR:
                    status = "error_timeout";
                    break;
                case LEVEL_SWITCH_ERROR:
                    status = "error_level_switch";
                    break;
                default:
                    status = "error_unknown";
            }
            break;
    }
    
    mqtt.publish(status_topic, status.c_str(), true);
}

void startMixing() {
    if (currentState != IDLE) return;
    
    // Start all relays (active LOW)
    digitalWrite(RELAY_AUGER, LOW);
    digitalWrite(RELAY_AGITATOR, LOW);
    digitalWrite(RELAY_MIXER, LOW);
    digitalWrite(RELAY_WATER, LOW);
    
    currentState = MIXING;
    mixingStartTime = millis();
    // Reset debounce for new mixing cycle
    levelSwitchDebounceStart = 0;
    levelSwitchStable = false;
    reportStatus();
}

void stopMixing() {
    // Turn off all relays (active LOW)
    digitalWrite(RELAY_AUGER, HIGH);
    digitalWrite(RELAY_AGITATOR, HIGH);
    digitalWrite(RELAY_MIXER, HIGH);
    digitalWrite(RELAY_WATER, HIGH);
    
    currentState = IDLE;
    reportStatus();
}

void handleError(ErrorType error) {
    stopMixing();
    currentState = ERROR;
    currentError = error;
    
    // Report error via MQTT (only if connected)
    if (mqtt.connected()) {
        String errorMessage;
        switch (error) {
            case TIMEOUT_ERROR:
                errorMessage = "Mixing timeout exceeded";
                break;
            case LEVEL_SWITCH_ERROR:
                errorMessage = "Level switch malfunction";
                break;
            default:
                errorMessage = "Unknown error";
                break;
        }
        
        mqtt.publish(error_topic, errorMessage.c_str(), true);
    }
    reportStatus();
    errorStartTime = millis(); // Start error timeout
}

void watchdogCheck() {
    // Check for level switch issues
    static unsigned long levelSwitchStuckTime = 0;
    static bool levelSwitchPrevState = false;
    bool currentLevelState = digitalRead(LEVEL_SWITCH) == LOW;
    
    if (currentState == MIXING && currentLevelState == levelSwitchPrevState) {
        if (levelSwitchStuckTime == 0) {
            levelSwitchStuckTime = millis();
        } else if (timeElapsed(levelSwitchStuckTime, 60000)) { // 1 minute stuck
            handleError(LEVEL_SWITCH_ERROR);
            levelSwitchStuckTime = 0;
        }
    } else {
        levelSwitchStuckTime = 0;
    }
    levelSwitchPrevState = currentLevelState;
}

bool timeElapsed(unsigned long start, unsigned long interval) {
    // Handle millis() overflow safely (occurs every ~49 days)
    unsigned long current = millis();
    if (current >= start) {
        return (current - start) >= interval;
    } else {
        // Overflow occurred, calculate correctly
        return (current + (0xFFFFFFFF - start) + 1) >= interval;
    }
}
