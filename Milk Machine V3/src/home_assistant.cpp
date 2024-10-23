#include "home_assistant.h"

// Global client to handle MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void setupHomeAssistant() {
    Serial.println("Setting up Home Assistant integration...");
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(callback);
    reconnect();  // Attempt to connect on startup
}

void reconnect() {
    // Loop until connected to the MQTT broker
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
            Serial.println("Connected to MQTT broker.");
            mqttClient.publish(MQTT_STATE_TOPIC, "Milk Mixer connected");
        } else {
            Serial.print("Connection failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println("; retrying in 5 seconds.");
            delay(5000);  // Wait 5 seconds before retrying
        }
    }
}

void loopHomeAssistant() {
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();  // Keep the connection alive and process messages
}

void updateHomeAssistant(const char* state, int hopperLevel) {
    if (mqttClient.connected()) {
        // Publish the state
        mqttClient.publish(MQTT_STATE_TOPIC, state);

        // Publish the hopper level as a string
        char hopperLevelStr[8];
        snprintf(hopperLevelStr, sizeof(hopperLevelStr), "%d", hopperLevel);
        mqttClient.publish(MQTT_HOPPER_TOPIC, hopperLevelStr);

        Serial.print("Updated Home Assistant with state: ");
        Serial.print(state);
        Serial.print(", Hopper Level: ");
        Serial.println(hopperLevel);
    } else {
        Serial.println("Failed to update Home Assistant: not connected.");
    }
}
