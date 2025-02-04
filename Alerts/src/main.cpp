#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>

#define VERSION "1.0.0"
#define LED_PIN 15
#define SPEAKER_PIN 18

// Network credentials and settings
struct WiFiNetwork {
    const char* ssid;
    const char* password;
};

// Add your known networks here
const WiFiNetwork networks[] = {
    {"WiFi", "Gliders1!"},
    {"WiFi-5G", "Gliders1!"},
    // Add more networks as needed
};
const int numNetworks = sizeof(networks) / sizeof(networks[0]);

IPAddress local_IP(192, 168, 1, 21);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// LED PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

// Breathing effect parameters
bool breathing = false;
unsigned long breathingStartTime = 0;
const unsigned long breathingDuration = 10000; // 10 seconds
const unsigned long breathePeriod = 2000; // 2 seconds per breath cycle

// Alert state
enum AlertType {
  NONE,
  CCTV,
  FIRE
};
AlertType currentAlert = NONE;

// Fire alert timing
unsigned long lastFirePatternTime = 0;
const unsigned long FIRE_PATTERN_INTERVAL = 2000; // 2 seconds per complete pattern

AsyncWebServer server(80);

// Function declarations
void setupWiFi();
void connectToBestAP();
void setupOTA();
void setupLED();
void handleCCTVAlert();
void handleFireAlert();
void stopAlert();
void startBreathingEffect();
void stopBreathingEffect();
void updateBreathingLed();
void playBeep(bool urgent = false);

void setup() {
  Serial.begin(115200);
  
  setupLED();
  pinMode(SPEAKER_PIN, OUTPUT);
  
  setupWiFi();
  setupOTA();
  
  // HTTP endpoints
  server.on("/trigger/cctv", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleCCTVAlert();    
    request->send(200, "application/json", "{\"status\":\"success\",\"alert\":\"cctv\"}");
  });

  server.on("/trigger/fire", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleFireAlert();
    request->send(200, "application/json", "{\"status\":\"success\",\"alert\":\"fire\"}");
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    stopAlert();
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"alerts cleared\"}");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String status;
    switch(currentAlert) {
      case CCTV: status = "cctv"; break;
      case FIRE: status = "fire"; break;
      default: status = "none";
    }
    
    // Get WiFi signal strength
    int rssi = WiFi.RSSI();
    // Convert RSSI to percentage (typically -100dBm to 0dBm)
    int signalPercentage = map(constrain(rssi, -100, -50), -100, -50, 0, 100);
    
    String response = "{\"active\":" + String(currentAlert != NONE) + 
                     ",\"type\":\"" + status + "\"" +
                     ",\"wifi\":{" +
                     "\"rssi\":" + String(rssi) + "," +
                     "\"strength\":" + String(signalPercentage) + "," +
                     "\"ip\":\"" + WiFi.localIP().toString() + "\"" +
                     "}}";
    request->send(200, "application/json", response);
  });

  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"version\":\"" VERSION "\"}");
  });

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  updateBreathingLed();
  
  // Handle continuous fire alert
  if (currentAlert == FIRE) {
    unsigned long currentTime = millis();
    if (currentTime - lastFirePatternTime >= FIRE_PATTERN_INTERVAL) {
      lastFirePatternTime = currentTime;
      playBeep(true);
      ledcWrite(ledChannel, 255); // Ensure LED stays at full brightness
    }
  }

  // Check WiFi connection every 30 seconds
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      connectToBestAP();
    }
  }
}

void setupWiFi() {
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("WiFi Static IP Configuration Failed");
  }

  connectToBestAP();
}

void connectToBestAP() {
  int bestSignal = -100; // Minimum RSSI value
  int bestNetworkIndex = -1;

  Serial.println("Scanning for available networks...");
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("No networks found");
    delay(5000);
    return;
  }

  // Find the strongest known network
  for (int i = 0; i < n; ++i) {
    String currentSSID = WiFi.SSID(i);
    int currentSignal = WiFi.RSSI(i);
    
    Serial.printf("Found network: %s (RSSI: %d)\n", currentSSID.c_str(), currentSignal);
    
    // Check if this is one of our known networks
    for (int j = 0; j < numNetworks; j++) {
      if (currentSSID == networks[j].ssid && currentSignal > bestSignal) {
        bestSignal = currentSignal;
        bestNetworkIndex = j;
        break;
      }
    }
  }

  // Clean up scan results
  WiFi.scanDelete();

  if (bestNetworkIndex != -1) {
    Serial.printf("Connecting to best network: %s (RSSI: %d)\n", 
                 networks[bestNetworkIndex].ssid, bestSignal);
    
    WiFi.begin(networks[bestNetworkIndex].ssid, networks[bestNetworkIndex].password);
    
    // Wait up to 10 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected");
      Serial.println("IP address: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nFailed to connect. Will retry later.");
    }
  } else {
    Serial.println("No known networks found with good signal. Will retry later.");
  }
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
}

void setupLED() {
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(LED_PIN, ledChannel);
  ledcWrite(ledChannel, 0); // Start with LED off
}

void handleCCTVAlert() {
  currentAlert = CCTV;
  startBreathingEffect();
  playBeep(false);
}

void handleFireAlert() {
  currentAlert = FIRE;
  stopBreathingEffect();
  ledcWrite(ledChannel, 255); // Full brightness
  lastFirePatternTime = millis();
  playBeep(true);
}

void stopAlert() {
  currentAlert = NONE;
  stopBreathingEffect();
  ledcWrite(ledChannel, 0);
  noTone(SPEAKER_PIN);
}

void startBreathingEffect() {
  breathing = true;
  breathingStartTime = millis();
}

void stopBreathingEffect() {
  breathing = false;
  ledcWrite(ledChannel, 0);
}

void updateBreathingLed() {
  if (!breathing) return;

  unsigned long elapsedTime = millis() - breathingStartTime;
  if (elapsedTime < breathingDuration) {
    float breathePhase = (elapsedTime % breathePeriod) / (float)breathePeriod;
    int brightness = (int)((exp(sin(breathePhase * PI)) - 0.36787944) * (255 / exp(1)));
    ledcWrite(ledChannel, brightness);
  } else {
    stopBreathingEffect();
  }
}

void playBeep(bool urgent) {
  if (urgent) {
    // Urgent pattern for fire alarm
    tone(SPEAKER_PIN, 2000, 300);
    delay(200);
    tone(SPEAKER_PIN, 2000, 300);
    delay(200);
    tone(SPEAKER_PIN, 2000, 300);
    delay(200);
    tone(SPEAKER_PIN, 2000, 300);
    delay(200);
    tone(SPEAKER_PIN, 2000, 300);
    delay(200);
  } else {
    // Normal pattern for CCTV
    tone(SPEAKER_PIN, 1000, 500);
    delay(500);
    tone(SPEAKER_PIN, 1500, 500);
    delay(500);
    tone(SPEAKER_PIN, 2000, 500);
  }
  delay(500);
  noTone(SPEAKER_PIN);
}
