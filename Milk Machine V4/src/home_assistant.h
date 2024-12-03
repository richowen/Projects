// home_assistant.h

#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_USERNAME "richowen"
#define MQTT_PASSWORD "p"
#define MQTT_CLIENT_ID "milk_mixer_esp32"
#define MQTT_STATE_TOPIC "homeassistant/sensor/milk_mixer/state"
#define MQTT_HOPPER_TOPIC "homeassistant/sensor/milk_mixer/hopper_level"
#define MQTT_TOTAL_MIXES_TOPIC "homeassistant/sensor/milk_mixer/total_mixes"
#define MQTT_TOTAL_RUNTIME_TOPIC "homeassistant/sensor/milk_mixer/total_runtime"
#define MQTT_ERROR_COUNT_TOPIC "homeassistant/sensor/milk_mixer/error_count"
#define MQTT_UPTIME_TOPIC "homeassistant/sensor/milk_mixer/current_uptime"

// Global MQTT client object
extern PubSubClient mqttClient;

// Setup Home Assistant integration (initialize MQTT)
void setupHomeAssistant();

// Main loop for Home Assistant (keeps the MQTT connection alive)
void loopHomeAssistant();

// Update the state and hopper level in Home Assistant
void updateHomeAssistant(const char *state);

// Update system statistics in Home Assistant
void updateHomeAssistantStats(uint32_t totalMixes, uint32_t totalRuntime, uint32_t errorCount, uint32_t currentUptime);

// Reconnect to MQTT broker if the connection is lost
void reconnect();

#endif // HOME_ASSISTANT_H
