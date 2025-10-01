#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "config.h"
#include "state_machine.h"
#include "logger.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFi.h>

class MQTTManager : public LogObserver {
private:
    Logger& _logger;
    WiFiClient _espClient;
    PubSubClient _client;
    SystemState _currentState = SystemState::IDLE;
    unsigned long _lastPublish = 0;
    bool _connected = false;

public:
    MQTTManager(Logger& logger)
        : _logger(logger), _client(_espClient) {
        _logger.addObserver(this);
    }

    ~MQTTManager() {
        _logger.removeObserver(this);
    }

    void initialize() {
        _logger.info("Initializing MQTT manager...");
        _client.setServer(MQTTConfig::SERVER, MQTTConfig::PORT);
        _client.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });
        _logger.info("MQTT manager initialized.");
    }

    void update(SystemState state, const InputStates& inputStates) {
        if (WiFi.status() != WL_CONNECTED) return;

        if (!_client.connected()) {
            reconnect();
        }

        _client.loop();

        // Publish status periodically or on state change
        if (state != _currentState ||
            (millis() - _lastPublish) >= MQTTConfig::PUBLISH_INTERVAL_MS) {
            publishStatus(state, inputStates);
            _currentState = state;
            _lastPublish = millis();
        }
    }

    void onLogMessage(const String& message) override {}

private:
    void reconnect() {
        if (_client.connect("MilkMachineV5", MQTTConfig::USER, MQTTConfig::PASSWORD)) {
            _logger.info("MQTT: Connected");
            _client.subscribe((String(MQTTConfig::BASE_TOPIC) + "/command").c_str());
            publishDiscovery();
            _connected = true;
        }
    }

    void publishDiscovery() {
        // Simplified discovery for main sensor
        String configTopic = "homeassistant/sensor/milk_machine/state/config";
        String payload = "{";
        payload += "\"name\":\"Milk Machine State\",";
        payload += "\"state_topic\":\"" + String(MQTTConfig::BASE_TOPIC) + "/state\",";
        payload += "\"unique_id\":\"milk_machine_state\"";
        payload += "}";
        _client.publish(configTopic.c_str(), payload.c_str(), true);
    }

    void publishStatus(SystemState state, const InputStates& inputStates) {
        if (!_client.connected()) return;

        const char* stateStr;
        switch (state) {
            case SystemState::IDLE: stateStr = "IDLE"; break;
            case SystemState::MIXING: stateStr = "MIXING"; break;
            case SystemState::POST_MIX: stateStr = "POST_MIX"; break;
            case SystemState::PERIODIC_MIX: stateStr = "PERIODIC_MIX"; break;
            case SystemState::WASH_STANDBY: stateStr = "WASH_STANDBY"; break;
            case SystemState::FAULT: stateStr = "FAULT"; break;
        }

        _client.publish((String(MQTTConfig::BASE_TOPIC) + "/state").c_str(), stateStr);
        _client.publish((String(MQTTConfig::BASE_TOPIC) + "/level").c_str(),
                       inputStates.levelSwitch == LOW ? "LOW" : "HIGH");
    }

    void mqttCallback(char* topic, byte* payload, unsigned int length) {
        String message;
        for (unsigned int i = 0; i < length; i++) {
            message += (char)payload[i];
        }

        _logger.printf("MQTT: Received %s: %s\n", topic, message.c_str());

        // Could emit events here for command processing
    }
};

#endif // MQTT_MANAGER_H