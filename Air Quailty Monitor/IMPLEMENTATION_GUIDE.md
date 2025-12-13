# ESP32 Air Quality Monitor - Implementation Guide

## Project Summary

This guide provides step-by-step instructions for implementing a complete air quality monitoring system with:
- ESP32 firmware reading 4 I2C sensors
- MQTT publishing to Home Assistant with auto-discovery
- Rainmeter desktop widget for real-time display

## Prerequisites

### Hardware
- [x] ESP32 DEVKIT 1
- [x] PM2.5 Sensor (DFRobot SEN0460) @ I2C 0x19
- [x] ENS160 Multi-Gas Sensor @ I2C 0x53
- [x] CO Gas Sensor @ I2C 0x74
- [x] O2 Gas Sensor @ I2C 0x76
- [x] I2C connections properly wired

### Software
- [x] PlatformIO installed
- [x] Home Assistant running at 192.168.1.3
- [x] MQTT broker configured
- [ ] Rainmeter installed (for widget phase)

### Network
- [x] WiFi network "WiFi" available
- [x] Static IP 192.168.1.25 reserved for ESP32
- [x] MQTT broker accessible at 192.168.1.3:1883

## Phase 1: ESP32 Firmware Development

### Step 1: Configure PlatformIO Dependencies

Update [`platformio.ini`](platformio.ini) with required libraries:

```ini
lib_deps = 
    dfrobot/DFRobot_AirQualitySensor@^1.0.0
    dfrobot/DFRobot_ENS160@^1.0.1
    phzi/DFRobot_MultiGasSensor@^2.0.0
    knolleary/PubSubClient@^2.8
    ; WiFi is built-in to ESP32 Arduino core
```

### Step 2: Create Configuration Header

Create [`include/config.h`](include/config.h):

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "WiFi"
#define WIFI_PASSWORD "Gliders!"
#define STATIC_IP IPAddress(192, 168, 1, 25)
#define GATEWAY IPAddress(192, 168, 1, 1)
#define SUBNET IPAddress(255, 255, 255, 0)
#define DNS IPAddress(8, 8, 8, 8)

// MQTT Configuration
#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_USER "richowen"
#define MQTT_PASSWORD "p"
#define MQTT_CLIENT_ID "esp32_air_quality_sensor"

// Sensor I2C Addresses
#define PM25_I2C_ADDR 0x19
#define ENS160_I2C_ADDR 0x53
#define CO_I2C_ADDR 0x74
#define O2_I2C_ADDR 0x76

// Update Interval (milliseconds)
#define UPDATE_INTERVAL 30000  // 30 seconds

// Serial Baud Rate
#define SERIAL_BAUD 115200

#endif
```

### Step 3: Main Firmware Structure

The [`src/main.cpp`](src/main.cpp) will be structured as follows:

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "DFRobot_AirQualitySensor.h"
#include "DFRobot_ENS160.h"
#include "DFRobot_MultiGasSensor.h"
#include "config.h"

// Global Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DFRobot_AirQualitySensor pm25(PM25_I2C_ADDR);
DFRobot_ENS160_I2C ens160(&Wire, ENS160_I2C_ADDR);
DFRobot_GAS_I2C coSensor(&Wire, CO_I2C_ADDR);
DFRobot_GAS_I2C o2Sensor(&Wire, O2_I2C_ADDR);

// Timing
unsigned long lastUpdate = 0;

// Function Declarations
void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void initSensors();
void publishDiscovery();
void readAndPublishSensors();
void publishSensor(const char* topic, float value);

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();
  
  setupWiFi();
  setupMQTT();
  initSensors();
  publishDiscovery();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    readAndPublishSensors();
    lastUpdate = millis();
  }
}
```

### Step 4: WiFi Connection Manager

```cpp
void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.config(STATIC_IP, GATEWAY, SUBNET, DNS);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
```

### Step 5: MQTT Connection Manager

```cpp
void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      publishDiscovery();  // Re-publish on reconnect
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}
```

### Step 6: Sensor Initialization

```cpp
void initSensors() {
  Serial.println("Initializing sensors...");
  
  // PM2.5 Sensor
  if (pm25.begin()) {
    Serial.println("PM2.5 sensor initialized");
  } else {
    Serial.println("PM2.5 sensor failed to initialize");
  }
  
  // ENS160 Sensor
  if (ens160.begin() == NO_ERR) {
    ens160.setPWRMode(ENS160_STANDARD_MODE);
    Serial.println("ENS160 sensor initialized");
  } else {
    Serial.println("ENS160 sensor failed to initialize");
  }
  
  // CO Sensor
  coSensor.begin(ADDRESS_3);
  coSensor.changeAcquireMode(coSensor.PASSIVITY);
  coSensor.setTempCompensation(coSensor.OFF);
  Serial.println("CO sensor initialized");
  
  // O2 Sensor
  o2Sensor.begin(ADDRESS_3);
  o2Sensor.changeAcquireMode(o2Sensor.PASSIVITY);
  o2Sensor.setTempCompensation(o2Sensor.OFF);
  Serial.println("O2 sensor initialized");
}
```

### Step 7: MQTT Auto-Discovery Payloads

Each sensor needs a discovery config published to:
`homeassistant/sensor/{unique_id}/config`

Example for PM2.5:
```json
{
  "name": "Air Quality PM2.5",
  "unique_id": "esp32_aq_pm25",
  "state_topic": "homeassistant/sensor/esp32_air_quality_pm25/state",
  "unit_of_measurement": "μg/m³",
  "device_class": "pm25",
  "device": {
    "identifiers": ["esp32_air_quality_sensor"],
    "name": "ESP32 Air Quality Monitor",
    "manufacturer": "DFRobot",
    "model": "Multi-Sensor Array"
  }
}
```

### Step 8: Reading and Publishing Sensors

```cpp
void readAndPublishSensors() {
  Serial.println("Reading sensors...");
  
  // Read PM2.5 Sensor
  uint16_t pm1 = pm25.readPM1_0();
  uint16_t pm25_value = pm25.readPM2_5();
  uint16_t pm10 = pm25.readPM10();
  
  // Read ENS160
  uint16_t tvoc = ens160.getTVOC();
  uint16_t eco2 = ens160.getECO2();
  uint8_t aqi = ens160.getAQI();
  
  // Read CO Sensor
  float co = coSensor.readGasConcentrationPPM();
  
  // Read O2 Sensor
  float o2 = o2Sensor.readGasConcentrationPPM();
  
  // Publish to MQTT
  publishSensor("homeassistant/sensor/esp32_air_quality_pm1/state", pm1);
  publishSensor("homeassistant/sensor/esp32_air_quality_pm25/state", pm25_value);
  publishSensor("homeassistant/sensor/esp32_air_quality_pm10/state", pm10);
  publishSensor("homeassistant/sensor/esp32_air_quality_tvoc/state", tvoc);
  publishSensor("homeassistant/sensor/esp32_air_quality_eco2/state", eco2);
  publishSensor("homeassistant/sensor/esp32_air_quality_aqi/state", aqi);
  publishSensor("homeassistant/sensor/esp32_air_quality_co/state", co);
  publishSensor("homeassistant/sensor/esp32_air_quality_o2/state", o2);
  
  // Serial logging
  Serial.printf("PM1.0: %d μg/m³\n", pm1);
  Serial.printf("PM2.5: %d μg/m³\n", pm25_value);
  Serial.printf("PM10: %d μg/m³\n", pm10);
  Serial.printf("TVOC: %d ppb\n", tvoc);
  Serial.printf("eCO2: %d ppm\n", eco2);
  Serial.printf("AQI: %d\n", aqi);
  Serial.printf("CO: %.2f ppm\n", co);
  Serial.printf("O2: %.2f%%\n", o2);
}
```

## Phase 2: Home Assistant Integration

### Step 1: Verify Auto-Discovery

After flashing ESP32:
1. Check MQTT broker for messages
2. Open Home Assistant → Settings → Devices & Services
3. Look for "ESP32 Air Quality Monitor" under MQTT integration
4. Verify all 8 sensors are discovered

### Step 2: Create Dashboard

Add sensors to Home Assistant dashboard:
```yaml
type: entities
title: Air Quality Monitor
entities:
  - entity: sensor.air_quality_pm1
  - entity: sensor.air_quality_pm25
  - entity: sensor.air_quality_pm10
  - entity: sensor.air_quality_tvoc
  - entity: sensor.air_quality_eco2
  - entity: sensor.air_quality_aqi
  - entity: sensor.air_quality_co
  - entity: sensor.air_quality_o2
```

### Step 3: Generate Long-Lived Access Token

1. Profile → Security → Long-Lived Access Tokens
2. Create token for Rainmeter
3. Save token securely

## Phase 3: Rainmeter Widget Development

### Step 1: Install Rainmeter
Download from https://www.rainmeter.net/

### Step 2: Create Skin Files

See [`RAINMETER_PLAN.md`](RAINMETER_PLAN.md) for complete implementation.

Directory structure:
```
Documents/Rainmeter/Skins/AirQuality/
├── AirQuality.ini
├── Variables.inc
└── @Resources/
```

### Step 3: Configure Variables

Edit `Variables.inc`:
```ini
HAHost=192.168.1.3
HAPort=8123
HAToken=YOUR_TOKEN_HERE
```

### Step 4: Load and Test

1. Right-click Rainmeter tray icon
2. Manage → Refresh all
3. Load AirQuality skin
4. Verify data appears

## Testing Checklist

### ESP32 Firmware
- [ ] Compiles without errors
- [ ] Uploads successfully
- [ ] WiFi connects with static IP
- [ ] All 4 sensors initialize
- [ ] MQTT connects successfully
- [ ] Auto-discovery payloads published
- [ ] Sensor readings appear in Serial Monitor
- [ ] 30-second updates working
- [ ] WiFi reconnection tested (router reboot)
- [ ] MQTT reconnection tested (broker restart)

### Home Assistant
- [ ] All 8 sensors auto-discovered
- [ ] Sensor names are clear and descriptive
- [ ] Units of measurement correct
- [ ] Values update every 30 seconds
- [ ] Historical data being recorded
- [ ] Sensors appear in dashboards

### Rainmeter Widget
- [ ] Skin loads without errors
- [ ] All sensor values display
- [ ] Updates every 30 seconds
- [ ] Color coding works for thresholds
- [ ] Connection error handling works
- [ ] Visual layout is clean

## Troubleshooting

### ESP32 Won't Connect to WiFi
- Verify SSID and password in config.h
- Check router allows static IP 192.168.1.25
- Try connecting without static IP first

### MQTT Not Connecting
- Verify broker IP and port
- Check firewall on Home Assistant host
- Test with MQTT Explorer tool

### Sensors Not Initializing
- Run I2C scanner to verify addresses
- Check wiring (SDA, SCL, VCC, GND)
- Verify sensors are powered (3.3V or 5V as required)

### Home Assistant Not Discovering Sensors
- Check MQTT integration is enabled
- Verify auto-discovery prefix is "homeassistant"
- Check MQTT broker logs

### Rainmeter Shows No Data
- Verify access token is correct
- Check network connectivity to HA
- Use browser to test API: http://192.168.1.3:8123/api/states
- Check WebParser RegExp patterns

## Next Steps

1. **Start with ESP32 firmware implementation** (switch to Code mode)
2. Test hardware connections
3. Verify Home Assistant integration
4. Develop Rainmeter widget
5. Fine-tune and optimize

## Additional Resources

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture and data flow
- [RAINMETER_PLAN.md](RAINMETER_PLAN.md) - Detailed Rainmeter implementation
- DFRobot sensor documentation (links in info file)
- Home Assistant MQTT Discovery: https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
- PubSubClient library: https://github.com/knolleary/pubsubclient

## Project Files Overview

```
Air Quality Monitor/
├── platformio.ini          # PlatformIO configuration
├── include/
│   └── config.h           # WiFi and MQTT credentials
├── src/
│   ├── main.cpp           # Main firmware code
│   └── info               # Project requirements
├── ARCHITECTURE.md         # System design document
├── RAINMETER_PLAN.md      # Rainmeter widget plan
└── IMPLEMENTATION_GUIDE.md # This file
```

Ready to begin implementation!