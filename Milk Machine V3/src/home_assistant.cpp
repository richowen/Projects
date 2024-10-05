// home_assistant.cpp

#include "home_assistant.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming messages here if needed
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setupHomeAssistant() {
  Serial.println("Setting up Home Assistant integration...");
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish(MQTT_STATE_TOPIC, "Milk Mixer connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loopHomeAssistant() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
}

void updateHomeAssistant(const char* state, int hopperLevel) {
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_STATE_TOPIC, state);
    
    // Convert hopper level to string and publish
    char hopperStr[8];
    snprintf(hopperStr, sizeof(hopperStr), "%d", hopperLevel);
    mqttClient.publish(MQTT_HOPPER_TOPIC, hopperStr);
    
    Serial.print("Updated Home Assistant state: ");
    Serial.print(state);
    Serial.print(", Hopper level: ");
    Serial.println(hopperLevel);
  } else {
    Serial.println("Failed to update Home Assistant: not connected");
  }
}