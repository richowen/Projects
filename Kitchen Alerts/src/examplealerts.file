#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <math.h>

// Network credentials and settings
const char *ssid = "WiFi";
const char *password = "Gliders1!";
IPAddress local_IP(192, 168, 1, 21);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // Google's DNS
IPAddress secondaryDNS(8, 8, 4, 4); // Google's DNS

// Server and pin settings
AsyncWebServer server(80);
const int ledPin = 15;
const int speakerPin = 18;

// Timing settings
const unsigned long reconnectInterval = 20000; // 30 seconds

// LED settings
const int ledChannel = 0;
const int freq = 800;
const int ledResolution = 8;
const int breathePeriod = 2000; // Total period for one breathe cycle (ms)
const unsigned long breathingDuration = 5000; // Duration of breathing effect in milliseconds

// Status variables
bool wifiConnected = false;
bool breathing = false;
unsigned long lastFlashTime = 0;
const int flashDuration = 500; // Flash every 500 ms
unsigned long breathingStartTime = 0; // Time when the breathing effect started

// Function prototypes
void flashLed();
void setupBreathingLed();
void startBreathingEffect();
void stopBreathingEffect();
void updateBreathingLed();
void playBeep();
void connectToWifi();
void triggerAlert();

// Task handles
TaskHandle_t wifiTaskHandle;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  connectToWifi(); // Attempt to connect to Wi-Fi on setup

  // Set up server routes
  server.on("/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
    triggerAlert();
    request->send(200, "text/plain", "Trigger received");
  });

  server.begin(); // Start server

  // Create WiFi connection task
  xTaskCreatePinnedToCore(
      [] (void * pvParameters) {
        for (;;) {
          connectToWifi();
          vTaskDelay(reconnectInterval / portTICK_PERIOD_MS);
        }
      },
      "WiFiTask",
      4096,
      NULL,
      1,
      &wifiTaskHandle,
      1); // Run on core 1
}

void loop() {
  if (!wifiConnected) {
    flashLed(); // Flash LED if not connected
  } else {
    digitalWrite(ledPin, LOW); // Ensure the LED is turned off when connected
  }

  updateBreathingLed(); // Update breathing LED effect
}

// Refactored and modularized functions below

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
  tone(speakerPin, 1000, 500);
  delay(500);
  tone(speakerPin, 1500, 500);
  delay(500);
  tone(speakerPin, 2000, 500);
  delay(500);
  noTone(speakerPin);
}

// Trigger alert actions
void triggerAlert() {
  playBeep();
  startBreathingEffect();
}

// Connects to the Wi-Fi network
void connectToWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    return;
  }

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // Wait for connection result with a timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < reconnectInterval) {
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay for 100ms
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi. Attempting again...");
    wifiConnected = false;
  } else {
    wifiConnected = true;
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  }
}
