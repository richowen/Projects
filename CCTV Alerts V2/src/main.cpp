#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// Pin definitions
const int SPEAKER_PIN = 13;
const int LED_PIN = 14;

// PWM configuration
const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8;  // 8-bit for low power

// Tone frequencies (Hz)
const int CCTV_TONE_1 = 800;
const int CCTV_TONE_2 = 1000;
const int CCTV_TONE_3 = 1200;
const int FIRE_TONE_HIGH = 1500;
const int FIRE_TONE_LOW = 1000;

// Timing constants (ms)
const int CCTV_FLASH_DURATION = 500;
const int FIRE_FLASH_DURATION = 200;

// WiFi credentials and network config
const char* ssid = "IoT";
const char* password = "Gliders1!";
IPAddress local_IP(192, 168, 1, 21);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Web server
AsyncWebServer server(80);

// Alert state
volatile bool fireAlertActive = false;

void playTone(int frequency, int duration) {
  if (frequency > 0) {
    ledcWriteTone(PWM_CHANNEL, frequency);
    ledcWrite(PWM_CHANNEL, 128);  // 50% duty cycle
  } else {
    ledcWrite(PWM_CHANNEL, 0);  // Silent
  }
  delay(duration);
  ledcWrite(PWM_CHANNEL, 0);  // Turn off after duration
}

void flashLED(int duration) {
  digitalWrite(LED_PIN, HIGH);
  delay(duration);
  digitalWrite(LED_PIN, LOW);
}

void handleCCTVAlert() {
  Serial.println("CCTV Alert triggered!");
  
  // Tone 1 + Flash 1
  ledcWriteTone(PWM_CHANNEL, CCTV_TONE_1);
  ledcWrite(PWM_CHANNEL, 128);
  digitalWrite(LED_PIN, HIGH);
  delay(CCTV_FLASH_DURATION);
  ledcWrite(PWM_CHANNEL, 0);
  digitalWrite(LED_PIN, LOW);
  delay(CCTV_FLASH_DURATION);
  
  // Tone 2 + Flash 2
  ledcWriteTone(PWM_CHANNEL, CCTV_TONE_2);
  ledcWrite(PWM_CHANNEL, 128);
  digitalWrite(LED_PIN, HIGH);
  delay(CCTV_FLASH_DURATION);
  ledcWrite(PWM_CHANNEL, 0);
  digitalWrite(LED_PIN, LOW);
  delay(CCTV_FLASH_DURATION);
  
  // Tone 3 + Flash 3
  ledcWriteTone(PWM_CHANNEL, CCTV_TONE_3);
  ledcWrite(PWM_CHANNEL, 128);
  digitalWrite(LED_PIN, HIGH);
  delay(CCTV_FLASH_DURATION);
  ledcWrite(PWM_CHANNEL, 0);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("CCTV Alert complete");
}

void handleFireAlert() {
  Serial.println("FIRE ALERT TRIGGERED! Continuous alarm active.");
  fireAlertActive = true;
  
  // Continuous loop - only exits on device reset
  while (true) {
    // High tone + LED ON
    ledcWriteTone(PWM_CHANNEL, FIRE_TONE_HIGH);
    ledcWrite(PWM_CHANNEL, 128);
    digitalWrite(LED_PIN, HIGH);
    delay(FIRE_FLASH_DURATION);
    
    // Low tone + LED OFF
    ledcWriteTone(PWM_CHANNEL, FIRE_TONE_LOW);
    ledcWrite(PWM_CHANNEL, 128);
    digitalWrite(LED_PIN, LOW);
    delay(FIRE_FLASH_DURATION);
    
    // Feed the watchdog to prevent reset
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\nESP32 CCTV Alert System Starting...");
  
  // Configure pins
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Configure PWM for speaker
  ledcSetup(PWM_CHANNEL, 1000, PWM_RESOLUTION);
  ledcAttachPin(SPEAKER_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);  // Start silent
  
  // Configure WiFi with static IP
  Serial.print("Configuring WiFi with static IP...");
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Brief confirmation beep
    playTone(1000, 100);
    delay(100);
    playTone(1200, 100);
  } else {
    Serial.println("\nWiFi connection failed!");
    // Error indication - rapid beeps
    for (int i = 0; i < 5; i++) {
      playTone(500, 100);
      delay(100);
    }
  }
  
  // Configure web server endpoints
  server.on("/trigger/cctv", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleCCTVAlert();
    request->send(200, "text/plain", "CCTV Alert Triggered");
  });
  
  server.on("/trigger/cctv", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleCCTVAlert();
    request->send(200, "text/plain", "CCTV Alert Triggered");
  });
  
  server.on("/trigger/fire", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "FIRE Alert Triggered - Device will alarm continuously");
    handleFireAlert();  // This never returns
  });
  
  server.on("/trigger/fire", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "FIRE Alert Triggered - Device will alarm continuously");
    handleFireAlert();  // This never returns
  });
  
  // Root endpoint for testing
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", 
      "<html><body>"
      "<h1>ESP32 CCTV Alert System</h1>"
      "<p>Status: Online</p>"
      "<p>Endpoints:</p>"
      "<ul>"
      "<li><a href='/trigger/cctv'>/trigger/cctv</a> - CCTV Alert (3 tones + 3 flashes)</li>"
      "<li><a href='/trigger/fire'>/trigger/fire</a> - Fire Alert (continuous)</li>"
      "</ul>"
      "</body></html>"
    );
  });
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Endpoints available:");
  Serial.println("  - http://192.168.1.21/trigger/cctv");
  Serial.println("  - http://192.168.1.21/trigger/fire");
  Serial.println("\nReady for alerts!");
}

void loop() {
  // Check WiFi connection status
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {  // Check every 10 seconds
    lastCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Attempting to reconnect...");
      WiFi.begin(ssid, password);
    }
  }
  
  // Just keep things running - web server handles requests asynchronously
  delay(100);
}
