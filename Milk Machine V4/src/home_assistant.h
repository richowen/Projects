// home_assistant.h

#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// MQTT Configuration
#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_USERNAME "richowen"
#define MQTT_PASSWORD "p"
#define MQTT_CLIENT_ID "milk_mixer_esp32"

// MQTT Topics
#define MQTT_STATE_TOPIC "homeassistant/sensor/milk_mixer/state"
#define MQTT_HOPPER_TOPIC "homeassistant/sensor/milk_mixer/hopper_level"
#define MQTT_AUGER_CURRENT_TOPIC "homeassistant/sensor/milk_mixer/auger_current"
#define MQTT_MIXER_CURRENT_TOPIC "homeassistant/sensor/milk_mixer/mixer_current"
#define MQTT_MIX_COUNT_TOPIC "homeassistant/sensor/milk_mixer/mix_count"
#define MQTT_ERROR_TOPIC "homeassistant/sensor/milk_mixer/error"
#define MQTT_COMMAND_TOPIC "homeassistant/sensor/milk_mixer/command"
#define MQTT_AVAILABILITY_TOPIC "homeassistant/sensor/milk_mixer/available"

// Home Assistant Discovery Topics
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_DEVICE_ID "milk_mixer"
#define HA_DEVICE_NAME "Milk Mixer"

// Global MQTT client object
extern PubSubClient mqttClient;

// Setup Home Assistant integration (initialize MQTT)
void setupHomeAssistant();

// Main loop for Home Assistant (keeps the MQTT connection alive)
void loopHomeAssistant();

// Update the state in Home Assistant
void updateHomeAssistant(const char *state);

// Update sensor values
void updateSensorValue(const char* topic, float value, const char* unit = nullptr);
void updateMixCount(int count);
void updateMotorCurrents(float augerCurrent, float mixerCurrent);
void reportError(const char* errorMessage);

// Send discovery messages to Home Assistant
void sendDiscoveryMessages();

// Reconnect to MQTT broker if the connection is lost
void reconnect();

// Handle incoming MQTT messages
void callback(char* topic, byte* payload, unsigned int length);

#endif // HOME_ASSISTANT_H
