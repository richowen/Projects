// home_assistant.h

#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define MQTT_SERVER "homeassistant.local"
#define MQTT_PORT 1883
#define MQTT_USERNAME "richowen"
#define MQTT_PASSWORD "p"
#define MQTT_CLIENT_ID "milk_mixer_esp32"
#define MQTT_STATE_TOPIC "homeassistant/sensor/milk_mixer/state"
#define MQTT_HOPPER_TOPIC "homeassistant/sensor/milk_mixer/hopper_level"

extern PubSubClient mqttClient;

void setupHomeAssistant();
void loopHomeAssistant();
void updateHomeAssistant(const char* state, int hopperLevel);
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);

#endif // HOME_ASSISTANT_H