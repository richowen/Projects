#ifndef MILK_MACHINE_APP_H
#define MILK_MACHINE_APP_H

#include "config.h"
#include "logger.h"
#include "hardware_controller.h"
#include "input_manager.h"
#include "state_machine.h"
#include "lcd_display.h"
#include "mqtt_manager.h"
#include "system_monitor.h"
#include "web_ota_manager.h"
#include <WiFi.h>
#include <WiFiServer.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

class MilkMachineApp {
private:
    // Core components
    Logger _logger;
    HardwareController _hardware;
    InputManager _inputManager;
    StateMachine _stateMachine;
    LCDDisplay _lcd;
    MQTTManager _mqtt;
    SystemMonitor _monitor;
    WebOTAManager _webOTA;

    // Network components
    WiFiServer _telnetServer;
    WiFiClient _telnetClient;
    unsigned long _lastStatusPrint = 0;

    // WiFi reconnection
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _wifiRetryInterval = WiFiReconnectConfig::RETRY_INIT_MS;

public:
    MilkMachineApp()
        : _hardware(_logger),
          _inputManager(_logger, _hardware),
          _stateMachine(_logger, _hardware),
          _lcd(_logger),
          _mqtt(_logger),
          _monitor(_logger),
          _webOTA(_logger),
          _telnetServer(23) {}

    void setup() {
        Serial.begin(115200);
        delay(50);

        _logger.info("\n=== Milk Machine V5 Modern Architecture ===");

        // Initialize components
        _hardware.initialize();
        _inputManager.initialize();
        _stateMachine.initialize();
        _lcd.initialize();
        _mqtt.initialize();

        // Setup event callbacks
        _inputManager.addEventCallback([this](SystemEvent event) {
            _stateMachine.processEvent(event);
        });

        _stateMachine.addStateChangeCallback([this](SystemState oldState, SystemState newState) {
            _lcd.update(newState);
        });

        // Initialize web OTA
        _webOTA.initialize();

        // Network setup
        initializeNetwork();

        // Watchdog
        esp_task_wdt_init(TimingConfig::WDT_TIMEOUT_SECONDS, true);
        esp_task_wdt_add(NULL);

        _logger.info("=== System Ready ===");
    }

    void loop() {
        esp_task_wdt_reset();

        // Update components
        _inputManager.update();
        _stateMachine.update();
        _monitor.update(_stateMachine.getCurrentState(), _stateMachine.getStateTime());
        _mqtt.update(_stateMachine.getCurrentState(), _inputManager.getStates());
        _webOTA.update();

        // Network services
        handleWiFiReconnection();
        handleTelnet();

        // Periodic status
        if ((millis() - _lastStatusPrint) >= TimingConfig::STATUS_INTERVAL_MS) {
            printSystemStatus();
            _lastStatusPrint = millis();
        }
    }

private:
    void initializeNetwork() {
        _logger.info("Initializing network...");

        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);

        // Static IP configuration
        if (!WiFi.config(
            IPAddressFromString(NetworkConfig::STATIC_IP),
            IPAddressFromString(NetworkConfig::GATEWAY),
            IPAddressFromString(NetworkConfig::SUBNET),
            IPAddressFromString(NetworkConfig::DNS1),
            IPAddressFromString(NetworkConfig::DNS2)
        )) {
            _logger.warning("Static IP config failed");
        }

        WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);
        _lastReconnectAttempt = millis();

        // OTA setup
        ArduinoOTA.setHostname(OTAConfig::HOSTNAME);
        ArduinoOTA.setPassword(OTAConfig::PASSWORD);
        ArduinoOTA.begin();

        _logger.info("Network initialized.");
    }

    void handleWiFiReconnection() {
        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.handle();
            _wifiRetryInterval = WiFiReconnectConfig::RETRY_INIT_MS;
            return;
        }

        unsigned long now = millis();
        if ((now - _lastReconnectAttempt) >= _wifiRetryInterval) {
            _logger.info("WiFi reconnecting...");
            WiFi.disconnect(false, false);
            WiFi.begin(NetworkConfig::WIFI_SSID, NetworkConfig::WIFI_PASSWORD);
            _lastReconnectAttempt = now;

            _wifiRetryInterval *= 2;
            if (_wifiRetryInterval > WiFiReconnectConfig::RETRY_MAX_MS) {
                _wifiRetryInterval = WiFiReconnectConfig::RETRY_MAX_MS;
            }
        }
    }

    void handleTelnet() {
        if (WiFi.status() != WL_CONNECTED) return;

        if (_telnetServer.hasClient()) {
            if (_telnetClient && _telnetClient.connected()) {
                _telnetClient.stop();
            }
            _telnetClient = _telnetServer.available();
            if (_telnetClient) {
                _logger.setTelnetClient(&_telnetClient);
                _telnetClient.println("\n=== Milk Machine V5 Telnet Monitor ===");
                printSystemStatus("Telnet client connected");
            }
        }

        if (_telnetClient && !_telnetClient.connected()) {
            _logger.setTelnetClient(nullptr);
            _telnetClient = WiFiClient();
        }
    }

    void printSystemStatus(const String& reason = "") {
        _logger.info("=== System Status ===");

        SystemState state = _stateMachine.getCurrentState();
        String stateStr;
        switch (state) {
            case SystemState::IDLE: stateStr = "IDLE"; break;
            case SystemState::MIXING: stateStr = "MIXING"; break;
            case SystemState::POST_MIX: stateStr = "POST_MIX"; break;
            case SystemState::PERIODIC_MIX: stateStr = "PERIODIC_MIX"; break;
            case SystemState::WASH_STANDBY: stateStr = "WASH_STANDBY"; break;
            case SystemState::FAULT: stateStr = "FAULT"; break;
        }

        _logger.info("State: " + stateStr);
        _logger.info("Level Switch: " +
                    String(_inputManager.getStates().levelSwitch == LOW ? "LOW (milk needed)" : "HIGH (milk sufficient)"));

        if (WiFi.status() == WL_CONNECTED) {
            _logger.info("WiFi: Connected (IP: " + WiFi.localIP().toString() + ")");
            _logger.info("Telnet: " + String(_telnetClient.connected() ? "Client connected" : "No client"));
        } else {
            _logger.info("WiFi: Disconnected");
        }

        if (reason.length() > 0) {
            _logger.info("Note: " + reason);
        }

        _logger.printf("Uptime: %lu s\n", millis() / 1000UL);
        _logger.printf("Free Heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
        _logger.info("====================");
    }

    IPAddress IPAddressFromString(const char* str) {
        IPAddress addr;
        addr.fromString(str);
        return addr;
    }
};

#endif // MILK_MACHINE_APP_H