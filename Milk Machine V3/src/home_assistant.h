// home_assistant.h

#ifndef HOME_ASSISTANT_H     // Include guard to prevent multiple inclusions of this header file
#define HOME_ASSISTANT_H

#include <Arduino.h>           // Include Arduino core library for basic functions
#include <PubSubClient.h>      // Include library for MQTT client functionality
#include <WiFi.h>              // Include library for WiFi connectivity

// Define constants for MQTT connection and topics
#define MQTT_SERVER "homeassistant.local"    // MQTT server address
#define MQTT_PORT 1883                       // MQTT server port
#define MQTT_USERNAME "richowen"             // Username for MQTT authentication
#define MQTT_PASSWORD "p"                    // Password for MQTT authentication
#define MQTT_CLIENT_ID "milk_mixer_esp32"    // Unique client ID for MQTT connection
#define MQTT_STATE_TOPIC "homeassistant/sensor/milk_mixer/state"  // Topic for publishing state
#define MQTT_HOPPER_TOPIC "homeassistant/sensor/milk_mixer/hopper_level" // Topic for publishing hopper level

extern PubSubClient mqttClient;              // Declare the MQTT client as an external variable

// Function prototypes for Home Assistant integration
void setupHomeAssistant();                   // Initializes the connection to Home Assistant via MQTT
void loopHomeAssistant();                    // Maintains the connection and handles incoming messages
void updateHomeAssistant(const char* state, int hopperLevel); // Publishes current state and hopper level to Home Assistant
void reconnect();                            // Reconnects to the MQTT server if connection is lost
void callback(char* topic, byte* payload, unsigned int length); // Callback function to handle incoming MQTT messages

#endif // HOME_ASSISTANT_H      // End of include guard
