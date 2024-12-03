#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ACS712.h>
#include "DFRobot_RGBLCD1602.h"

// WiFi credentials
const char* ssid = "WiFi";
const char* password = "Gliders1!";

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
const unsigned long MIXING_TIMEOUT = 600000; // 1 minutes max mixing time
const unsigned long POST_MIX_TIME = 5000;   // 5 seconds post-mix time
const unsigned long ERROR_RETRY_DELAY = 300000; // 5 minutes between retries
const unsigned long CURRENT_REPORT_INTERVAL = 1000; // Report current every second
const unsigned long WATCHDOG_INTERVAL = 500;  // Check system every 500ms
const unsigned long LCD_UPDATE_INTERVAL = 500; // Update LCD every 500ms

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
unsigned long lastCurrentReport = 0;
unsigned long lastWatchdogCheck = 0;
unsigned long lastLCDUpdate = 0;
bool levelSwitchLastState = false;
int levelSwitchStableCount = 0;
float lastCurrentReading = 0.0;

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
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(handleMQTTMessage);
    
    // Initialize current sensor
    currentSensor.autoMidPoint();
    
    // Report initial status
    mqtt.publish(availability_topic, "online", true);
    reportStatus();
    
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
            reportStatus();
        }
        // Control water relay based on dispense switch
        digitalWrite(RELAY_WATER, digitalRead(WASH_DISPENSE_PIN) == LOW ? LOW : HIGH);
    } else if (currentState == WASH) {
        // Exit wash mode
        currentState = IDLE;
        digitalWrite(RELAY_WATER, HIGH);
        reportStatus();
    }

    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();

    // Regular current reporting
    if (millis() - lastCurrentReport >= CURRENT_REPORT_INTERVAL) {
        reportCurrent();
        lastCurrentReport = millis();
    }

    // LCD updates
    if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        updateLCD();
        lastLCDUpdate = millis();
    }

    // Watchdog checks
    if (millis() - lastWatchdogCheck >= WATCHDOG_INTERVAL) {
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
            
        case MIXING:
            if (millis() - mixingStartTime > MIXING_TIMEOUT) {
                handleError(TIMEOUT_ERROR);
                break;
            }
            
            checkCurrentDraw();
            
            if (digitalRead(LEVEL_SWITCH) == LOW) {  // Level reached
                // Debounce level switch
                if (!levelSwitchLastState) {
                    levelSwitchStableCount++;
                    if (levelSwitchStableCount >= 5) { // Must be stable for 2.5 seconds
                        currentState = POST_MIXING;
                        postMixStartTime = millis();
                        
                        // Turn off all except mixer
                        digitalWrite(RELAY_AUGER, HIGH);
                        digitalWrite(RELAY_AGITATOR, HIGH);
                        digitalWrite(RELAY_WATER, HIGH);
                        
                        levelSwitchStableCount = 0;
                        reportStatus();
                    }
                }
            } else {
                levelSwitchStableCount = 0;
            }
            levelSwitchLastState = (digitalRead(LEVEL_SWITCH) == LOW);
            break;
            
        case POST_MIXING:
            if (millis() - postMixStartTime >= POST_MIX_TIME) {
                stopMixing();
                currentState = IDLE;
                reportStatus();
            }
            break;
            
        case ERROR:
            if (millis() - mixingStartTime > ERROR_RETRY_DELAY) {
                currentState = IDLE;
                currentError = NO_ERROR;
                reportStatus();
            }
            break;
    }
}

void updateLCD() {
    String displayText;
    int remainingTime = 0;
    char currentStr[8];
    
    lcd.clear();
    
    // Update LCD color based on state
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
    
    // First line: State
    lcd.setCursor(0, 0);
    switch (currentState) {
        case IDLE:
            displayText = "Status: IDLE";
            break;
        case MIXING:
            displayText = "Status: MIXING";
            break;
        case POST_MIXING:
            remainingTime = (POST_MIX_TIME - (millis() - postMixStartTime)) / 1000;
            displayText = "Post-Mix: " + String(remainingTime) + "s";
            break;
        case ERROR:
            displayText = "ERROR";
            break;
        case WASH:
            displayText = "WASH MODE";
            break;
    }
    lcd.print(displayText);
    
    // Second line: Current reading or error message
    lcd.setCursor(0, 1);
    if (currentState == ERROR) {
        switch (currentError) {
            case EMPTY_POWDER:
                displayText = "Empty Powder!";
                break;
            case TIMEOUT_ERROR:
                displayText = "Timeout Error!";
                break;
            case MOTOR_OVERLOAD:
                displayText = "Motor Overload!";
                break;
            case LEVEL_SWITCH_ERROR:
                displayText = "Level Sw Error!";
                break;
            default:
                displayText = "Unknown Error!";
        }
        lcd.print(displayText);
    } else if (currentState == MIXING || currentState == POST_MIXING) {
        dtostrf(lastCurrentReading, 1, 1, currentStr);
        displayText = "Current: " + String(currentStr) + "A";
        lcd.print(displayText);
    } else if (currentState == WASH) {
        lcd.print(digitalRead(WASH_DISPENSE_PIN) == LOW ? "Water: ON" : "Water: OFF");
    } else {
        lcd.print("Ready to Start");
    }
}

void setupWiFi() {
    lcd.clear();
    lcd.print("Connecting WiFi");
    
    delay(10);
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        lcd.setCursor(dots, 1);
        lcd.print(".");
        dots = (dots + 1) % 16;
    }
    
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
}

void reconnectMQTT() {
    while (!mqtt.connected()) {
        Serial.println("Attempting MQTT connection...");
        lcd.clear();
        lcd.print("Connecting MQTT");
        
        String clientId = "MilkMixer-";
        clientId += String(random(0xffff), HEX);
        
        if (mqtt.connect(clientId.c_str(), mqtt_user, mqtt_password, availability_topic, 1, true, "offline")) {
            Serial.println("MQTT connected");
            mqtt.subscribe(command_topic);
            mqtt.publish(availability_topic, "online", true);
            reportStatus();
            
            lcd.clear();
            lcd.print("MQTT Connected");
            delay(1000);
        } else {
            Serial.println("MQTT connection failed, retrying in 5 seconds");
            lcd.setCursor(0, 1);
            lcd.print("Failed, retry...");
            delay(5000);
        }
    }
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
    if (currentState == MIXING || currentState == POST_MIXING) {
        lastCurrentReading = currentSensor.mA_DC() / 1000.0; // Convert mA to A
        char currentStr[10];
        dtostrf(lastCurrentReading, 1, 2, currentStr);
        mqtt.publish(current_topic, currentStr, false);
    }
}

void checkCurrentDraw() {
    float current = currentSensor.mA_DC() / 1000.0; // Convert mA to A
    lastCurrentReading = current;
    
    // Check for empty powder (low current)
    if (current < EMPTY_CURRENT_THRESHOLD && currentState == MIXING) {
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
    levelSwitchStableCount = 0;
    levelSwitchLastState = false;
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
    
    // Report error via MQTT
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
    }
    
    mqtt.publish(error_topic, errorMessage.c_str(), true);
    reportStatus();
    mixingStartTime = millis(); // Start error timeout
}

void watchdogCheck() {
    // Check for level switch issues
    static unsigned long levelSwitchStuckTime = 0;
    static bool levelSwitchPrevState = false;
    bool currentLevelState = digitalRead(LEVEL_SWITCH) == LOW;
    
    if (currentState == MIXING && currentLevelState == levelSwitchPrevState) {
        if (levelSwitchStuckTime == 0) {
            levelSwitchStuckTime = millis();
        } else if (millis() - levelSwitchStuckTime > 60000) { // 1 minute stuck
            handleError(LEVEL_SWITCH_ERROR);
            levelSwitchStuckTime = 0;
        }
    } else {
        levelSwitchStuckTime = 0;
    }
    levelSwitchPrevState = currentLevelState;
}
