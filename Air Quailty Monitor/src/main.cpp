#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>
#include "DFRobot_AirQualitySensor.h"
#include "DFRobot_ENS160.h"
#include "DFRobot_MultiGasSensor.h"
#include "config.h"

// Global Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Sensor Objects
DFRobot_AirQualitySensor pm25Sensor(&Wire, PM25_I2C_ADDR);
DFRobot_ENS160_I2C ens160(&Wire, ENS160_I2C_ADDR);
DFRobot_GAS_I2C coSensor(&Wire, CO_I2C_ADDR);
DFRobot_GAS_I2C o2Sensor(&Wire, O2_I2C_ADDR);

// Sensor Status Flags
bool pm25Available = false;
bool ens160Available = false;
bool coAvailable = false;
bool o2Available = false;

// Timing
unsigned long lastUpdate = 0;
bool discoveryPublished = false;

// Function Declarations
void setupWiFi();
void reconnectWiFi();
void setupMQTT();
void reconnectMQTT();
void initSensors();
void publishDiscovery();
void publishSensorDiscovery(const char* sensorName, const char* uniqueId, 
                           const char* stateTopic, const char* unit, 
                           const char* deviceClass, const char* icon = nullptr);
void readAndPublishSensors();
void publishSensor(const char* topic, float value);
void publishSensor(const char* topic, int value);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 Air Quality Monitor");
  Serial.println("=================================\n");
  
  Wire.begin();
  Serial.println("I2C initialized");
  
  setupWiFi();
  setupMQTT();
  initSensors();
  
  Serial.println("\nSetup complete!");
  Serial.println("=================================\n");
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
  
  // Check MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Publish discovery on first successful MQTT connection
  if (mqttClient.connected() && !discoveryPublished) {
    publishDiscovery();
    discoveryPublished = true;
  }
  
  // Read and publish sensor data every UPDATE_INTERVAL
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    readAndPublishSensors();
    lastUpdate = millis();
  }
}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.config(STATIC_IP, GATEWAY, SUBNET, DNS);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Test broker reachability
    Serial.print("  Testing broker (");
    Serial.print(MQTT_SERVER);
    Serial.print(")... ");
    
    IPAddress brokerIP;
    if (brokerIP.fromString(MQTT_SERVER)) {
      if (Ping.ping(brokerIP, 3)) {
        Serial.println("OK");
      } else {
        Serial.println("FAILED");
      }
    }
  } else {
    Serial.println("✗ WiFi connection failed!");
  }
}

void reconnectWiFi() {
  Serial.println("WiFi lost, reconnecting...");
  WiFi.disconnect();
  delay(1000);
  setupWiFi();
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  Serial.print("MQTT broker: ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT... ");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected!");
      discoveryPublished = false;
    } else {
      Serial.print("failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), retrying in 5s");
      delay(5000);
    }
  }
}

void initSensors() {
  Serial.println("\nInitializing sensors...");
  Serial.println("-------------------------");
  
  // PM2.5 Sensor
  Serial.print("PM2.5 (0x");
  Serial.print(PM25_I2C_ADDR, HEX);
  Serial.print("): ");
  pm25Available = pm25Sensor.begin();
  Serial.println(pm25Available ? "OK" : "FAILED");
  
  // ENS160 Sensor
  Serial.print("ENS160 (0x");
  Serial.print(ENS160_I2C_ADDR, HEX);
  Serial.print("): ");
  if (ens160.begin() == NO_ERR) {
    ens160.setPWRMode(ENS160_STANDARD_MODE);
    ens160Available = true;
    Serial.println("OK");
  } else {
    Serial.println("FAILED");
  }
  
  // CO Sensor
  Serial.print("CO (0x");
  Serial.print(CO_I2C_ADDR, HEX);
  Serial.print("): ");
  if (coSensor.begin()) {
    coSensor.changeAcquireMode(coSensor.PASSIVITY);
    coSensor.setTempCompensation(coSensor.OFF);
    delay(100);
    coAvailable = (coSensor.readGasConcentrationPPM() >= 0);
    Serial.println(coAvailable ? "OK" : "FAILED");
  } else {
    Serial.println("FAILED");
  }
  
  // O2 Sensor
  Serial.print("O2 (0x");
  Serial.print(O2_I2C_ADDR, HEX);
  Serial.print("): ");
  if (o2Sensor.begin()) {
    o2Sensor.changeAcquireMode(o2Sensor.PASSIVITY);
    o2Sensor.setTempCompensation(o2Sensor.OFF);
    delay(100);
    o2Available = (o2Sensor.readGasConcentrationPPM() >= 0);
    Serial.println(o2Available ? "OK" : "FAILED");
  } else {
    Serial.println("FAILED");
  }
  
  Serial.println("-------------------------");
  int count = (pm25Available ? 1 : 0) + (ens160Available ? 1 : 0) + 
              (coAvailable ? 1 : 0) + (o2Available ? 1 : 0);
  Serial.print("Active sensors: ");
  Serial.print(count);
  Serial.println(" of 4");
}

void publishDiscovery() {
  Serial.println("\nPublishing Home Assistant discovery...");
  
  // PM2.5 Sensor - 3 measurements
  if (pm25Available) {
    publishSensorDiscovery("Air Quality PM1.0", "esp32_aq_pm1", 
                          "homeassistant/sensor/esp32_air_quality_pm1/state",
                          "μg/m³", "pm1", "mdi:air-filter");
    
    publishSensorDiscovery("Air Quality PM2.5", "esp32_aq_pm25", 
                          "homeassistant/sensor/esp32_air_quality_pm25/state",
                          "μg/m³", "pm25", "mdi:air-filter");
    
    publishSensorDiscovery("Air Quality PM10", "esp32_aq_pm10", 
                          "homeassistant/sensor/esp32_air_quality_pm10/state",
                          "μg/m³", "pm10", "mdi:air-filter");
  }
  
  // ENS160 Sensor - 3 measurements
  if (ens160Available) {
    publishSensorDiscovery("Air Quality TVOC", "esp32_aq_tvoc", 
                          "homeassistant/sensor/esp32_air_quality_tvoc/state",
                          "ppb", "volatile_organic_compounds", "mdi:molecule");
    
    publishSensorDiscovery("Air Quality eCO2", "esp32_aq_eco2", 
                          "homeassistant/sensor/esp32_air_quality_eco2/state",
                          "ppm", "carbon_dioxide", "mdi:molecule-co2");
    
    publishSensorDiscovery("Air Quality AQI", "esp32_aq_aqi", 
                          "homeassistant/sensor/esp32_air_quality_aqi/state",
                          "", "aqi", "mdi:air-filter");
  }
  
  // CO Sensor
  if (coAvailable) {
    publishSensorDiscovery("Air Quality CO", "esp32_aq_co", 
                          "homeassistant/sensor/esp32_air_quality_co/state",
                          "ppm", "carbon_monoxide", "mdi:molecule-co");
  }
  
  // O2 Sensor
  if (o2Available) {
    publishSensorDiscovery("Air Quality O2", "esp32_aq_o2", 
                          "homeassistant/sensor/esp32_air_quality_o2/state",
                          "%", "", "mdi:air-filter");
  }
  
  Serial.println("Discovery complete!");
}

void publishSensorDiscovery(const char* sensorName, const char* uniqueId, 
                           const char* stateTopic, const char* unit, 
                           const char* deviceClass, const char* icon) {
  StaticJsonDocument<512> doc;
  
  doc["name"] = sensorName;
  doc["unique_id"] = uniqueId;
  doc["state_topic"] = stateTopic;
  
  if (strlen(unit) > 0) {
    doc["unit_of_measurement"] = unit;
  }
  
  if (strlen(deviceClass) > 0) {
    doc["device_class"] = deviceClass;
  }
  
  if (icon != nullptr) {
    doc["icon"] = icon;
  }
  
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"][0] = "esp32_air_quality_sensor";
  device["name"] = "ESP32 Air Quality Monitor";
  device["manufacturer"] = "DFRobot";
  device["model"] = "Multi-Sensor Array";
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  char configTopic[128];
  snprintf(configTopic, sizeof(configTopic), "%s/%s/config", MQTT_BASE_TOPIC, uniqueId);
  
  mqttClient.publish(configTopic, buffer, true);
}

void readAndPublishSensors() {
  Serial.println("\n--- Reading Sensors ---");
  
  // PM2.5 Sensor
  if (pm25Available) {
    uint16_t pm1 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM1_0_STANDARD);
    uint16_t pm25 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM2_5_STANDARD);
    uint16_t pm10 = pm25Sensor.gainParticleConcentration_ugm3(PARTICLE_PM10_STANDARD);
    
    Serial.printf("PM1.0: %d | PM2.5: %d | PM10: %d μg/m³\n", pm1, pm25, pm10);
    
    publishSensor("homeassistant/sensor/esp32_air_quality_pm1/state", pm1);
    publishSensor("homeassistant/sensor/esp32_air_quality_pm25/state", pm25);
    publishSensor("homeassistant/sensor/esp32_air_quality_pm10/state", pm10);
  }
  
  // ENS160 Sensor
  if (ens160Available) {
    uint16_t tvoc = ens160.getTVOC();
    uint16_t eco2 = ens160.getECO2();
    uint8_t aqi = ens160.getAQI();
    
    Serial.printf("TVOC: %d ppb | eCO2: %d ppm | AQI: %d\n", tvoc, eco2, aqi);
    
    publishSensor("homeassistant/sensor/esp32_air_quality_tvoc/state", tvoc);
    publishSensor("homeassistant/sensor/esp32_air_quality_eco2/state", eco2);
    publishSensor("homeassistant/sensor/esp32_air_quality_aqi/state", aqi);
  }
  
  // CO Sensor
  if (coAvailable) {
    float co = coSensor.readGasConcentrationPPM();
    Serial.printf("CO: %.2f ppm\n", co);
    publishSensor("homeassistant/sensor/esp32_air_quality_co/state", co);
  }
  
  // O2 Sensor
  if (o2Available) {
    float o2 = o2Sensor.readGasConcentrationPPM();
    Serial.printf("O2: %.2f%%\n", o2);
    publishSensor("homeassistant/sensor/esp32_air_quality_o2/state", o2);
  }
  
  Serial.println("----------------------");
}

void publishSensor(const char* topic, float value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%.2f", value);
  mqttClient.publish(topic, buffer, true);
}

void publishSensor(const char* topic, int value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", value);
  mqttClient.publish(topic, buffer, true);
}