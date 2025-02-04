#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <math.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>

// Network credentials and settings
const char* ssid = "WiFi";
const char* password = "Gliders1!";
IPAddress local_IP(192, 168, 1, 21);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // Google's DNS
IPAddress secondaryDNS(8, 8, 4, 4); // Google's DNS

// Server and pin settings
AsyncWebServer server(80);
const int ledPin = 15;
const int speakerPin = 18;

// System monitoring
unsigned long startTime = 0;
const int WDT_TIMEOUT = 30;  // Watchdog timeout in seconds
RTC_DATA_ATTR int bootCount = 0;
Preferences preferences;

// Timing settings
const unsigned long MIN_RECONNECT_INTERVAL = 5000;   // 5 seconds
const unsigned long MAX_RECONNECT_INTERVAL = 300000; // 5 minutes
unsigned long currentReconnectInterval = MIN_RECONNECT_INTERVAL;
unsigned long lastReconnectAttempt = 0;

// LED settings
const int ledChannel = 0;
const int freq = 800;
const int ledResolution = 8;
const int breathePeriod = 2000; // Total period for one breathe cycle (ms)
const unsigned long breathingDuration = 5000; // Duration of breathing effect in milliseconds

// Status variables
bool wifiConnected = false;
bool breathing = false;
bool fireAlarmActive = false;
unsigned long lastFlashTime = 0;
const int flashDuration = 500; // Flash every 500 ms
const int fireAlarmFlashDuration = 100; // Faster flash for fire alarm
unsigned long breathingStartTime = 0; // Time when the breathing effect started
unsigned long lastFireAlarmTone = 0;
const int fireAlarmToneInterval = 500; // Time between alarm tones

// System metrics
unsigned long lastHeapCheck = 0;
const unsigned long HEAP_CHECK_INTERVAL = 60000; // Check heap every minute
size_t lastFreeHeap = 0;

// Function prototypes
void flashLed();
void setupBreathingLed();
void startBreathingEffect();
void stopBreathingEffect();
void updateBreathingLed();
void playBeep();
void connectToWifi();
void triggerAlert();
void checkSystemHealth();
void startFireAlarm();
void stopFireAlarm();
void updateFireAlarm();

// Task handles
TaskHandle_t wifiTaskHandle;
TaskHandle_t healthTaskHandle;

void setup() {
  Serial.begin(115200);
  
  // Initialize watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  
  // Initialize preferences
  preferences.begin("cctv-alerts", false);
  
  // Track boot count and check for boot loops
  bootCount++;
  if (bootCount > 3 && millis() < 60000) {
    Serial.println("Possible boot loop detected!");
    delay(5000); // Give time for serial output
  }
  Serial.printf("Boot count: %d\n", bootCount);
  
  // Record start time for uptime calculation
  startTime = millis();
  lastFreeHeap = ESP.getFreeHeap();
  
  pinMode(ledPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  // Configure WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  // Set WiFi power save mode to NONE for better stability
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  connectToWifi(); // Attempt to connect to Wi-Fi on setup

  // Set up server routes
  server.on("/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
    triggerAlert();
    request->send(200, "text/plain", "Trigger received");
  });

  // Fire alarm endpoints
  server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request) {
    startFireAlarm();
    request->send(200, "text/plain", "Fire alarm activated");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    stopFireAlarm();
    request->send(200, "text/plain", "Fire alarm deactivated");
  });

  // Health check endpoint
  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    String health = "{\n";
    health += "  \"uptime\": " + String(millis() - startTime) + ",\n";
    health += "  \"wifi_strength\": " + String(WiFi.RSSI()) + ",\n";
    health += "  \"free_heap\": " + String(ESP.getFreeHeap()) + ",\n";
    health += "  \"boot_count\": " + String(bootCount) + ",\n";
    health += "  \"wifi_connected\": " + String(wifiConnected ? "true" : "false") + ",\n";
    health += "  \"heap_change\": " + String(ESP.getFreeHeap() - lastFreeHeap) + "\n";
    health += "}";
    request->send(200, "application/json", health);
  });

  server.begin(); // Start server

  // Create WiFi connection task
  xTaskCreatePinnedToCore(
      [] (void * pvParameters) {
        for (;;) {
          connectToWifi();
          esp_task_wdt_reset(); // Reset watchdog in WiFi task
          vTaskDelay(currentReconnectInterval / portTICK_PERIOD_MS);
        }
      },
      "WiFiTask",
      4096,
      NULL,
      1,
      &wifiTaskHandle,
      1); // Run on core 1

  // Create system health monitoring task
  xTaskCreatePinnedToCore(
      [] (void * pvParameters) {
        for (;;) {
          checkSystemHealth();
          esp_task_wdt_reset(); // Reset watchdog in health task
          vTaskDelay(HEAP_CHECK_INTERVAL / portTICK_PERIOD_MS);
        }
      },
      "HealthTask",
      4096,
      NULL,
      1, // Changed priority from 2 to 1
      &healthTaskHandle,
      0); // Run on core 0
}

void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();

  if (fireAlarmActive) {
    updateFireAlarm();
  } else if (!wifiConnected) {
    flashLed(); // Flash LED if not connected
  } else {
    digitalWrite(ledPin, LOW); // Ensure the LED is turned off when connected
  }

  updateBreathingLed(); // Update breathing LED effect
  
  // Small delay to prevent tight loop
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

// Fire alarm functions
void startFireAlarm() {
  fireAlarmActive = true;
  breathing = false; // Stop any breathing effect
  lastFireAlarmTone = 0; // Force immediate tone
  lastFlashTime = 0; // Force immediate LED flash
  digitalWrite(speakerPin, LOW);
  noTone(speakerPin);
}

void stopFireAlarm() {
  fireAlarmActive = false;
  digitalWrite(ledPin, LOW);
  digitalWrite(speakerPin, LOW);
  noTone(speakerPin);
}

void updateFireAlarm() {
  // Update LED
  unsigned long currentMillis = millis();
  if (currentMillis - lastFlashTime >= fireAlarmFlashDuration) {
    lastFlashTime = currentMillis;
    digitalWrite(ledPin, !digitalRead(ledPin));
  }

  // Update alarm sound
  if (currentMillis - lastFireAlarmTone >= fireAlarmToneInterval) {
    lastFireAlarmTone = currentMillis;
    static bool highTone = false;
    tone(speakerPin, highTone ? 2000 : 1500, fireAlarmToneInterval - 50);
    highTone = !highTone;
  }
}

// System health monitoring
void checkSystemHealth() {
  size_t currentFreeHeap = ESP.getFreeHeap();
  
  // Check for significant heap changes
  if (abs((int)(currentFreeHeap - lastFreeHeap)) > 1024) {
    Serial.printf("Significant heap change detected! Before: %d, After: %d\n", 
                 lastFreeHeap, currentFreeHeap);
  }
  
  // Check for low memory condition
  if (currentFreeHeap < 10000) {
    Serial.println("Warning: Low memory condition detected!");
  }
  
  lastFreeHeap = currentFreeHeap;
}

// Flashes the LED when not connected to Wi-Fi
void flashLed() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastFlashTime > flashDuration) {
    lastFlashTime = currentMillis;
    digitalWrite(ledPin, !digitalRead(ledPin)); // Toggle LED state
  }
}

// Sets up the PWM channel for the LED
void setupBreathingLed() {
  ledcSetup(ledChannel, freq, ledResolution);
  ledcAttachPin(ledPin, ledChannel);
}

// Starts the breathing effect on the LED
void startBreathingEffect() {
  setupBreathingLed();
  breathing = true;
  breathingStartTime = millis();
}

// Stops the breathing effect on the LED
void stopBreathingEffect() {
  breathing = false;
  ledcWrite(ledChannel, 0); // Turn off LED
}

// Updates the LED for a breathing effect
void updateBreathingLed() {
  if (!breathing) return;

  unsigned long elapsedTime = millis() - breathingStartTime;
  if (elapsedTime < breathingDuration) {
    float breathePhase = (elapsedTime % breathePeriod) / (float)breathePeriod;
    int brightness = (int)((exp(sin(breathePhase * PI)) - 0.36787944) * (255 / exp(1)));
    ledcWrite(ledChannel, brightness);
  } else {
    stopBreathingEffect(); // Stop after the duration
  }
}

// Play a series of beeps for the alert
void playBeep() {
  // Use non-blocking delays
  static const int tones[] = {1000, 1500, 2000};
  for (int i = 0; i < 3; i++) {
    tone(speakerPin, tones[i], 500);  // Reduced duration to ensure complete silence
    vTaskDelay(500 / portTICK_PERIOD_MS);
    noTone(speakerPin);  // Explicitly stop tone after each beep
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Small gap between beeps
  }
  digitalWrite(speakerPin, LOW);  // Ensure speaker is completely off
  noTone(speakerPin);  // Final cleanup
}

// Trigger alert actions
void triggerAlert() {
  playBeep();
  startBreathingEffect();
}

// Connects to the Wi-Fi network with improved error handling and exponential backoff
void connectToWifi() {
  unsigned long currentMillis = millis();
  
  // Check if it's time to attempt reconnection
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    currentReconnectInterval = MIN_RECONNECT_INTERVAL; // Reset interval on successful connection
    return;
  }
  
  if (currentMillis - lastReconnectAttempt < currentReconnectInterval) {
    return; // Not time to attempt reconnection yet
  }
  
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();
  lastReconnectAttempt = startAttemptTime;
  
  // Wait for connection result with a timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    esp_task_wdt_reset(); // Reset watchdog timer while attempting connection
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi. Will retry later...");
    wifiConnected = false;
    // Implement exponential backoff
    currentReconnectInterval = min(currentReconnectInterval * 2, MAX_RECONNECT_INTERVAL);
    Serial.printf("Next attempt in %d seconds\n", currentReconnectInterval / 1000);
  } else {
    wifiConnected = true;
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
}
