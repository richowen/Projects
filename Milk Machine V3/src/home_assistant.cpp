// home_assistant.cpp

#include "home_assistant.h"        // Include the header file for Home Assistant integration

WiFiClient espClient;             // Create a WiFi client for MQTT communication
PubSubClient mqttClient(espClient); // Create an MQTT client instance using the WiFi client

void callback(char * topic, byte * payload, unsigned int length) {
  // Callback function to handle incoming messages from Home Assistant
  Serial.print("Message arrived [");  // Print the topic of the incoming message
  Serial.print(topic);
  Serial.print("] ");
  // Print the message payload
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);  // Print each character of the payload
  }
  Serial.println();  // New line after the message
}

void setupHomeAssistant() {
  Serial.println("Setting up Home Assistant integration..."); // Print setup message
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT); // Set the MQTT server and port
  mqttClient.setCallback(callback); // Set the callback function for incoming messages
}

void reconnect() {
  // Loop until we are reconnected to the MQTT server
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection..."); // Print connection attempt message
    // Attempt to connect with MQTT credentials
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected"); // Print success message upon connection
      // Publish an announcement to Home Assistant after successful connection
      mqttClient.publish(MQTT_STATE_TOPIC, "Milk Mixer connected");
    } else {
      // Print failure message and error code if connection attempt fails
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state()); // Print MQTT state error code
      Serial.println(" try again in 5 seconds"); // Inform user about retry
      // Wait for 5 seconds before retrying
      delay(5000);
    }
  }
}

void loopHomeAssistant() {
  // Ensure the client is connected; reconnect if necessary
  if (!mqttClient.connected()) {
    reconnect(); // Attempt to reconnect if not connected
  }
  mqttClient.loop(); // Maintain the MQTT connection and handle incoming messages
}

void updateHomeAssistant(const char * state, int hopperLevel) {
  // Publish the current state and hopper level to Home Assistant
  if (mqttClient.connected()) { // Check if MQTT client is connected
    mqttClient.publish(MQTT_STATE_TOPIC, state); // Publish the current state

    // Convert hopper level integer to string for publishing
    char hopperStr[8]; // Buffer for hopper level string
    snprintf(hopperStr, sizeof(hopperStr), "%d", hopperLevel); // Format hopper level as string
    mqttClient.publish(MQTT_HOPPER_TOPIC, hopperStr); // Publish hopper level

    // Print updated state and hopper level to Serial Monitor
    Serial.print("Updated Home Assistant state: ");
    Serial.print(state);
    Serial.print(", Hopper level: ");
    Serial.println(hopperLevel);
  } else {
    // Print error message if not connected
    Serial.println("Failed to update Home Assistant: not connected");
  }
}
