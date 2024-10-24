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

// Global MQTT client object
extern PubSubClient mqttClient;

// Setup Home Assistant integration (initialize MQTT)
void setupHomeAssistant();

// Main loop for Home Assistant (keeps the MQTT connection alive)
void loopHomeAssistant();

// Update the state and hopper level in Home Assistant
void updateHomeAssistant(const char* state, int hopperLevel);

// Reconnect to MQTT broker if the connection is lost
void reconnect();

#endif // HOME_ASSISTANT_H
