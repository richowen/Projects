#include "home_assistant.h"

// Global client to handle MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void setupHomeAssistant()
{
    Serial.println("Setting up Home Assistant integration...");
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(callback);
    reconnect(); // Attempt to connect on startup
}

void reconnect()
{
    // Loop until connected to the MQTT broker
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
        {
            Serial.println("Connected to MQTT broker.");
            mqttClient.publish(MQTT_STATE_TOPIC, "Milk Mixer connected");
        }
        else
        {
            Serial.print("Connection failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println("; retrying in 5 seconds.");
            delay(5000); // Wait 5 seconds before retrying
        }
    }
}

void loopHomeAssistant()
{
    if (!mqttClient.connected())
    {
        reconnect();
    }
    mqttClient.loop(); // Keep the connection alive and process messages
}

void updateHomeAssistant(const char *state)
{
    if (mqttClient.connected())
    {
        // Publish the state
        mqttClient.publish(MQTT_STATE_TOPIC, state);
    }
    else
    {
        Serial.println("Failed to update Home Assistant: not connected.");
    }
}

void updateHomeAssistantStats(uint32_t totalMixes, uint32_t totalRuntime, uint32_t errorCount, uint32_t currentUptime)
{
    if (mqttClient.connected())
    {
        char buffer[16];  // Buffer for converting numbers to strings

        // Publish total mixes
        itoa(totalMixes, buffer, 10);
        mqttClient.publish(MQTT_TOTAL_MIXES_TOPIC, buffer);

        // Publish total runtime (in hours)
        float runtimeHours = totalRuntime / 3600.0;  // Convert seconds to hours
        dtostrf(runtimeHours, 4, 1, buffer);  // Convert float to string with 1 decimal place
        mqttClient.publish(MQTT_TOTAL_RUNTIME_TOPIC, buffer);

        // Publish error count
        itoa(errorCount, buffer, 10);
        mqttClient.publish(MQTT_ERROR_COUNT_TOPIC, buffer);

        // Publish current uptime (in hours)
        float uptimeHours = currentUptime / 3600.0;  // Convert seconds to hours
        dtostrf(uptimeHours, 4, 1, buffer);  // Convert float to string with 1 decimal place
        mqttClient.publish(MQTT_UPTIME_TOPIC, buffer);
    }
    else
    {
        Serial.println("Failed to update Home Assistant stats: not connected.");
    }
}
