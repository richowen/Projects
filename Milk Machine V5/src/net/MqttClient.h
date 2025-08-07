#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "../core/Config.h"
#include "../core/DataStore.h"

class MqttClientMM {
public:
    void begin(DataStore* storePtr) {
        store = storePtr;
        client.setClient(wifi);
        client.setServer(Net::MQTT_HOST, Net::MQTT_PORT);
        client.setCallback([](char*, uint8_t*, unsigned int) {
            // Monitoring only; no commands processed
        });
        lastAttempt = 0;
        lastReport = 0;
    }

    void loop() {
        if (!client.connected()) {
            reconnectIfDue();
        }
        if (client.connected()) {
            client.loop();
            // Periodic telemetry
            unsigned long now = millis();
            if (now - lastReport >= Times::DATA_REPORT_INTERVAL) {
                publishData();
                lastReport = now;
            }
        }
    }

    bool connected() { return client.connected(); }

    void publishStatusIdle()        { publishStatus("idle"); }
    void publishStatusMixing()      { publishStatus("mixing"); }
    void publishStatusPostMixing()  { publishStatus("post_mixing"); }
    void publishStatusWash()        { publishStatus("wash_mode"); }
    void publishErrorTimeout()      { publishStatus("error_timeout"); publishError("Mixing timeout exceeded"); }
    void publishErrorLevelSwitch()  { publishStatus("error_level_switch"); publishError("Level switch malfunction"); }
    void publishErrorUnknown()      { publishStatus("error_unknown"); publishError("Unknown error"); }

    void publishData() {
        if (!client.connected() || !store) return;

        char buf[24];

        ultoa(store->getTotalMixes(), buf, 10);
        client.publish(Net::T_DATA_TOTAL_MIXES, buf, false);

        ultoa(store->getSessionMixes(), buf, 10);
        client.publish(Net::T_DATA_SESSION_MIXES, buf, false);

        ultoa(store->getUptimeHours(), buf, 10);
        client.publish(Net::T_DATA_UPTIME_HOURS, buf, false);

        ultoa(store->getErrorCount(), buf, 10);
        client.publish(Net::T_DATA_ERROR_COUNT, buf, false);

        if (store->getLastMixTime() > 0) {
            ultoa(store->getLastMixTime(), buf, 10);
            client.publish(Net::T_DATA_LAST_MIX, buf, false);
        }
    }

    // Call on transitions
    void onEnterIdle()       { if (connected()) publishStatusIdle(); }
    void onEnterMixing()     { if (connected()) publishStatusMixing(); }
    void onEnterPostMix()    { if (connected()) publishStatusPostMixing(); }
    void onEnterWash()       { if (connected()) publishStatusWash(); }
    void onEnterErrorTimeout()     { if (connected()) publishErrorTimeout(); }
    void onEnterErrorLevelSwitch() { if (connected()) publishErrorLevelSwitch(); }
    void onEnterErrorUnknown()     { if (connected()) publishErrorUnknown(); }

private:
    WiFiClient wifi;
    PubSubClient client;
    DataStore* store{nullptr};
    unsigned long lastAttempt{0};
    unsigned long lastReport{0};

    void reconnectIfDue() {
        unsigned long now = millis();
        if (now - lastAttempt < Times::MQTT_RECONNECT_TIMEOUT) return;
        lastAttempt = now;

        String clientId = "MilkMixer-";
        clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

        if (client.connect(clientId.c_str(),
                           Net::MQTT_USER, Net::MQTT_PASS,
                           Net::T_AVAILABLE, 1, false, "offline")) {
            client.publish(Net::T_AVAILABLE, "online", false);
            publishStatusIdle();
            publishData(); // send initial blob
        }
    }

    void publishStatus(const char* s) {
        if (!client.connected()) return;
        client.publish(Net::T_STATUS, s, false);
    }

    void publishError(const char* msg) {
        if (!client.connected()) return;
        client.publish(Net::T_ERROR, msg, false);
    }
};