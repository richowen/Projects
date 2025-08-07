#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ACS712.h>
#include <ArduinoOTA.h>
#include "DFRobot_RGBLCD1602.h"

// WiFi credentials
const char* ssid = "WiFi";
const char* password = "Gliders1!";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 8);    // Static IP for ESP32
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
const char* current_topic = "milk_mixer/current";
const char* availability_topic = "milk_mixer/available";

// Pin Definitions
const int RELAY_AUGER = 25;      // Powder auger relay
const int RELAY_AGITATOR = 26;   // Powder agitator relay
const int RELAY_MIXER = 16;      // Liquid mixer relay
const int RELAY_WATER = 17;      // Water solenoid relay
const int LEVEL_SWITCH = 12;     // Pressure switch input
const int CURRENT_SENSOR = 35;   // ACS712 analog input
#define WASH_STANDBY_PIN 23      // Wash standby switch
#define WASH_DISPENSE_PIN 5      // Water solenoid activate switch in wash mode

// Constants
const float EMPTY_CURRENT_THRESHOLD = 0.2;  // Amps - adjust based on your motor
const float MAX_CURRENT_THRESHOLD = 2.0;   // Amps - adjust based on your motor
const unsigned long MIXING_TIMEOUT = 60000; // 1 minutes max mixing time
const unsigned long POST_MIX_TIME = 5000;   // 5 seconds post-mix time
const unsigned long ERROR_RETRY_DELAY = 300000; // 5 minutes between retries
const unsigned long CURRENT_REPORT_INTERVAL = 1000; // Report current every second
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
    EMPTY_POWDER,
    TIMEOUT_ERROR,
    MOTOR_OVERLOAD,
    LEVEL_SWITCH_ERROR
};

// Global Variables
SystemState currentState = IDLE;
ErrorType currentError = NO_ERROR;
unsigned long mixingStartTime = 0;
unsigned long postMixStartTime = 0;
unsigned long errorStartTime = 0;
unsigned long lastCurrentReport = 0;
unsigned long lastWatchdogCheck = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long levelSwitchDebounceStart = 0;
unsigned long washWaterStartTime = 0;
bool levelSwitchLastState = false;
bool levelSwitchStable = false;
bool washWaterActive = false;
float lastCurrentReading = 0.0;
bool currentSensorCalibrated = false;
String lastLCDLine1 = "";
String lastLCDLine2 = "";

// Objects
WiFiClient espClient;
PubSubClient mqtt(espClient);
ACS712 currentSensor(CURRENT_SENSOR, 5.0, 4095, 66); // 30A version, 5V, 12-bit ADC, mV/A
DFRobot_RGBLCD1602 lcd(/*RGBAddr*/0x2D, /*lcdCols*/16, /*lcdRows*/2);

// Function Declarations
void setupWiFi();
void reconnectMQTT();
void handleMQTTMessage(char* topic, byte* payload, unsigned int length);
void reportStatus();
void checkCurrentDraw();
void startMixing();
void stopMixing();
void handleError(ErrorType error);
void watchdogCheck();
void reportCurrent();
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
    
    // Initialize current sensor with validation
    lcd.clear();
    lcd.print("Calibrating...");
    lcd.setCursor(0, 1);
    lcd.print("Current Sensor");
    
    currentSensor.autoMidPoint();
    delay(1000); // Allow sensor to stabilize
    
    // Validate calibration
    float testReading = currentSensor.mA_DC() / 1000.0;
    if (testReading > -10.0 && testReading < 10.0) { // Reasonable range check
        currentSensorCalibrated = true;
        lcd.setCursor(0, 1);
        lcd.print("Sensor OK       ");
    } else {
        currentSensorCalibrated = false;
        lcd.setCursor(0, 1);
        lcd.print("Sensor Error!   ");
    }
    delay(1000);
    
    // Report initial status (only if MQTT is connected)
    if (mqtt.connected()) {
        mqtt.publish(availability_topic, "online", true);
        reportStatus();
    }
    
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

    // Regular current reporting
    if (timeElapsed(lastCurrentReport, CURRENT_REPORT_INTERVAL)) {
        reportCurrent();
        lastCurrentReport = millis();
    }

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
            
        case IDLE:
            // Nothing to do in idle state
            break;
            
        case MIXING: {
            if (timeElapsed(mixingStartTime, MIXING_TIMEOUT)) {
                handleError(TIMEOUT_ERROR);
                break;
            }
            
            checkCurrentDraw();
            
            // Level switch debouncing with time-based approach
            bool currentLevelState = digitalRead(LEVEL_SWITCH) == LOW;
            if (currentLevelState != levelSwitchLastState) {
                levelSwitchDebounceStart = millis();
                levelSwitchStable = false;
                levelSwitchLastState = currentLevelState;
            } else if (!levelSwitchStable && timeElapsed(levelSwitchDebounceStart, LEVEL_DEBOUNCE_TIME)) {
                levelSwitchStable = true;
                if (currentLevelState) { // Level reached and stable
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
    char currentStr[8];
    static SystemState lastState = IDLE;
    static ErrorType lastError = NO_ERROR;
    
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
            dtostrf(lastCurrentReading, 1, 1, currentStr);
            line2 = "Current: " + String(currentStr) + "A";
            break;
        case POST_MIXING: {
            int remainingTime = (POST_MIX_TIME - (millis() - postMixStartTime)) / 1000;
            line1 = "Post-Mix: " + String(remainingTime) + "s";
            dtostrf(lastCurrentReading, 1, 1, currentStr);
            line2 = "Current: " + String(currentStr) + "A";
            break;
        }
        case ERROR:
            line1 = "ERROR";
            switch (currentError) {
                case EMPTY_POWDER:
                    line2 = "Empty Powder!";
                    break;
                case TIMEOUT_ERROR:
                    line2 = "Timeout Error!";
                    break;
                case MOTOR_OVERLOAD:
                    line2 = "Motor Overload!";
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
    Serial.println("Configuring static IP...");
    
    // Configure static IP
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Static IP configuration failed");
        lcd.setCursor(0, 1);
        lcd.print("IP Config Failed");
        delay(2000);
    }
    
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

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
        mqtt.subscribe(command_topic);
        mqtt.publish(availability_topic, "online", true);
        reportStatus();
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqtt.state());
    }
    lastMQTTAttempt = millis();
}

void handleMQTTMessage(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    if (String(topic) == command_topic) {
        if (message == "start" && currentState == IDLE) {
            startMixing();
        } else if (message == "stop") {
            stopMixing();
        }
    }
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
                case EMPTY_POWDER:
                    status = "error_empty_powder";
                    break;
                case TIMEOUT_ERROR:
                    status = "error_timeout";
                    break;
                case MOTOR_OVERLOAD:
                    status = "error_motor_overload";
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

void reportCurrent() {
    if (!mqtt.connected()) return; // Only publish if MQTT is connected
    
    if (currentState == MIXING || currentState == POST_MIXING) {
        lastCurrentReading = currentSensor.mA_DC() / 1000.0; // Convert mA to A
        char currentStr[10];
        dtostrf(lastCurrentReading, 1, 2, currentStr);
        mqtt.publish(current_topic, currentStr, false);
    }
}

void checkCurrentDraw() {
    if (!currentSensorCalibrated) return; // Skip if sensor not calibrated
    
    float current = currentSensor.mA_DC() / 1000.0; // Convert mA to A
    
    // Validate reading is reasonable
    if (current < -50.0 || current > 50.0) return; // Skip invalid readings
    
    lastCurrentReading = current;
    
    // Check for empty powder (low current) - only after motor has had time to start
    if (current < EMPTY_CURRENT_THRESHOLD && currentState == MIXING && 
        timeElapsed(mixingStartTime, 5000)) { // Wait 5 seconds after start
        handleError(EMPTY_POWDER);
    }
    
    // Check for motor overload (high current)
    if (current > MAX_CURRENT_THRESHOLD) {
        handleError(MOTOR_OVERLOAD);
    }
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
    levelSwitchDebounceStart = 0;
    levelSwitchLastState = false;
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
            case EMPTY_POWDER:
                errorMessage = "Powder hopper empty";
                break;
            case TIMEOUT_ERROR:
                errorMessage = "Mixing timeout exceeded";
                break;
            case MOTOR_OVERLOAD:
                errorMessage = "Motor overload detected";
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
    return (millis() - start) >= interval;
}
