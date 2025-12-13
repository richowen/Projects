#include "network.h"
#include "config.h"
#include "alerts.h"
#include "system.h"
#include <esp_wifi.h>

// Global server instance
AsyncWebServer server(80);

// HTML template for configuration page
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>CCTV Alert System Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .alert-config { border: 1px solid #ccc; padding: 10px; margin: 10px 0; }
        input, select { margin: 5px; padding: 5px; }
        button { padding: 10px 20px; margin: 5px; }
    </style>
</head>
<body>
    <h1>CCTV Alert System Configuration</h1>
    <form id="configForm">
        <div id="alerts"></div>
        <button type="submit">Save Configuration</button>
    </form>
    <script>
        async function loadConfig() {
            const response = await fetch('/api/config');
            const configs = await response.json();
            const container = document.getElementById('alerts');

            configs.forEach((config, index) => {
                const div = document.createElement('div');
                div.className = 'alert-config';
                div.innerHTML = `
                    <h3>${config.name}</h3>
                    <label>LED Pattern:
                        <select name="pattern_${index}">
                            <option value="0" ${config.ledPattern == 0 ? 'selected' : ''}>Solid</option>
                            <option value="1" ${config.ledPattern == 1 ? 'selected' : ''}>Blink</option>
                            <option value="2" ${config.ledPattern == 2 ? 'selected' : ''}>Breathe</option>
                            <option value="3" ${config.ledPattern == 3 ? 'selected' : ''}>Strobe</option>
                        </select>
                    </label>
                    <label>Duration (ms): <input type="number" name="duration_${index}" value="${config.ledDuration}"></label>
                    <label>Priority:
                        <select name="priority_${index}">
                            <option value="0" ${config.priority == 0 ? 'selected' : ''}>Low</option>
                            <option value="1" ${config.priority == 1 ? 'selected' : ''}>Medium</option>
                            <option value="2" ${config.priority == 2 ? 'selected' : ''}>High</option>
                        </select>
                    </label>
                `;
                container.appendChild(div);
            });
        }

        document.getElementById('configForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const response = await fetch('/api/config', {
                method: 'POST',
                body: formData
            });
            alert(await response.text());
        });

        loadConfig();
    </script>
</body>
</html>
)rawliteral";

void initNetwork() {
    // Configure WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);

    esp_wifi_set_ps(WIFI_PS_NONE);

    // Set up server routes
    setupWebRoutes();

    server.begin();
    Serial.println("Web server started");
}

void setupWebRoutes() {
    // Alert trigger endpoints
    server.on("/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
        String typeParam = request->getParam("type", true)->value();
        AlertType alertType = ALERT_UNKNOWN;

        if (typeParam == "car") alertType = ALERT_CAR;
        else if (typeParam == "truck") alertType = ALERT_TRUCK;
        else if (typeParam == "motorcycle") alertType = ALERT_MOTORCYCLE;
        else if (typeParam == "pedestrian") alertType = ALERT_PEDESTRIAN;

        if (alertType != ALERT_UNKNOWN) {
            triggerAlert(alertType);
            logSystemEvent("Alert triggered: " + String(typeParam));
            sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Alert triggered: " + typeParam + "\"}");
        } else {
            sendErrorResponse(request, 400, "Invalid alert type");
        }
    });

    // Fire alarm endpoints
    server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request) {
        fireAlarmActive = true;
        breathing = false;
        lastFireAlarmTone = 0;
        lastFlashTime = 0;
        digitalWrite(SPEAKER_PIN, LOW);
        noTone(SPEAKER_PIN);
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Fire alarm activated\"}");
    });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        fireAlarmActive = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(SPEAKER_PIN, LOW);
        noTone(SPEAKER_PIN);
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Fire alarm deactivated\"}");
    });

    // Health check endpoint
    server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendJsonResponse(request, getSystemStatusJson());
    });

    // Configuration endpoints
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", CONFIG_HTML);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        for (int i = 0; i < ALERT_COUNT; i++) {
            if (i > 0) json += ",";
            AlertConfig config = getAlertConfig((AlertType)i);
            json += "{";
            json += "\"name\":\"" + config.name + "\",";
            json += "\"ledPattern\":" + String(config.ledPattern) + ",";
            json += "\"ledDuration\":" + String(config.ledDuration) + ",";
            json += "\"priority\":" + String(config.priority);
            json += "}";
        }
        json += "]";
        sendJsonResponse(request, json);
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Handle configuration updates
        int params = request->params();
        for (int i = 0; i < params; i++) {
            AsyncWebParameter* p = request->getParam(i);
            String name = p->name();
            String value = p->value();

            if (name.startsWith("pattern_")) {
                int index = name.substring(8).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.ledPattern = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            } else if (name.startsWith("duration_")) {
                int index = name.substring(9).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.ledDuration = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            } else if (name.startsWith("priority_")) {
                int index = name.substring(9).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.priority = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            }
        }

        saveAlertConfigs();
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Configuration saved\"}");
    });

    // Test endpoints
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
        String typeParam = request->getParam("type", true)->value();
        AlertType alertType = ALERT_UNKNOWN;

        if (typeParam == "car") alertType = ALERT_CAR;
        else if (typeParam == "truck") alertType = ALERT_TRUCK;
        else if (typeParam == "motorcycle") alertType = ALERT_MOTORCYCLE;
        else if (typeParam == "pedestrian") alertType = ALERT_PEDESTRIAN;
        else if (typeParam == "unknown") alertType = ALERT_UNKNOWN;

        if (alertType != ALERT_UNKNOWN || typeParam == "unknown") {
            triggerAlert(alertType);
            sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Test alert triggered: " + typeParam + "\"}");
        } else {
            sendErrorResponse(request, 400, "Invalid test type");
        }
    });
}

void setupWebRoutes() {
    // Alert trigger endpoints
    server.on("/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
        String typeParam = request->getParam("type", true)->value();
        AlertType alertType = ALERT_UNKNOWN;

        if (typeParam == "car") alertType = ALERT_CAR;
        else if (typeParam == "truck") alertType = ALERT_TRUCK;
        else if (typeParam == "motorcycle") alertType = ALERT_MOTORCYCLE;
        else if (typeParam == "pedestrian") alertType = ALERT_PEDESTRIAN;

        if (alertType != ALERT_UNKNOWN) {
            triggerAlert(alertType);
            logSystemEvent("Alert triggered: " + String(typeParam));
            sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Alert triggered: " + typeParam + "\"}");
        } else {
            sendErrorResponse(request, 400, "Invalid alert type");
        }
    });

    // Fire alarm endpoints
    server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request) {
        fireAlarmActive = true;
        breathing = false;
        lastFireAlarmTone = 0;
        lastFlashTime = 0;
        digitalWrite(SPEAKER_PIN, LOW);
        noTone(SPEAKER_PIN);
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Fire alarm activated\"}");
    });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        fireAlarmActive = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(SPEAKER_PIN, LOW);
        noTone(SPEAKER_PIN);
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Fire alarm deactivated\"}");
    });

    // Health check endpoint
    server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        sendJsonResponse(request, getSystemStatusJson());
    });

    // Configuration endpoints
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", CONFIG_HTML);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        for (int i = 0; i < ALERT_COUNT; i++) {
            if (i > 0) json += ",";
            AlertConfig config = getAlertConfig((AlertType)i);
            json += "{";
            json += "\"name\":\"" + config.name + "\",";
            json += "\"ledPattern\":" + String(config.ledPattern) + ",";
            json += "\"ledDuration\":" + String(config.ledDuration) + ",";
            json += "\"priority\":" + String(config.priority);
            json += "}";
        }
        json += "]";
        sendJsonResponse(request, json);
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Handle configuration updates
        int params = request->params();
        for (int i = 0; i < params; i++) {
            AsyncWebParameter* p = request->getParam(i);
            String name = p->name();
            String value = p->value();

            if (name.startsWith("pattern_")) {
                int index = name.substring(8).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.ledPattern = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            } else if (name.startsWith("duration_")) {
                int index = name.substring(9).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.ledDuration = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            } else if (name.startsWith("priority_")) {
                int index = name.substring(9).toInt();
                if (index >= 0 && index < ALERT_COUNT) {
                    AlertConfig config = getAlertConfig((AlertType)index);
                    config.priority = value.toInt();
                    setAlertConfig((AlertType)index, config);
                }
            }
        }

        saveAlertConfigs();
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Configuration saved\"}");
    });

    // Test endpoints
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
        String typeParam = request->getParam("type", true)->value();
        AlertType alertType = ALERT_UNKNOWN;

        if (typeParam == "car") alertType = ALERT_CAR;
        else if (typeParam == "truck") alertType = ALERT_TRUCK;
        else if (typeParam == "motorcycle") alertType = ALERT_MOTORCYCLE;
        else if (typeParam == "pedestrian") alertType = ALERT_PEDESTRIAN;
        else if (typeParam == "unknown") alertType = ALERT_UNKNOWN;

        if (alertType != ALERT_UNKNOWN || typeParam == "unknown") {
            triggerAlert(alertType);
            sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Test alert triggered: " + typeParam + "\"}");
        } else {
            sendErrorResponse(request, 400, "Invalid test type");
        }
    });
}

void connectToWifi() {
    unsigned long currentMillis = millis();

    // Check current connection
    if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.RSSI() < WIFI_RSSI_THRESHOLD) {
            Serial.println("Weak signal detected, searching for better AP...");
            WiFi.disconnect(true);
            wifiConnected = false;
        } else {
            wifiConnected = true;
            currentReconnectInterval = MIN_RECONNECT_INTERVAL;
            return;
        }
    }

    if (currentMillis - lastReconnectAttempt < currentReconnectInterval) {
        return;
    }

    Serial.println("Connecting to WiFi...");

    // Find and connect to strongest AP
    int8_t rssi = findStrongestAP();
    if (rssi == -127) {
        Serial.println("No suitable AP found");
        wifiConnected = false;
        currentReconnectInterval = min(currentReconnectInterval * 2, MAX_RECONNECT_INTERVAL);
        lastReconnectAttempt = currentMillis;
        return;
    }

    unsigned long startAttemptTime = millis();
    lastReconnectAttempt = startAttemptTime;

    // Wait for connection with timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_SCAN_TIMEOUT) {
        resetWatchdog();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi. Will retry later...");
        wifiConnected = false;
        currentReconnectInterval = min(currentReconnectInterval * 2, MAX_RECONNECT_INTERVAL);
        Serial.printf("Next attempt in %d seconds\n", currentReconnectInterval / 1000);
    } else {
        wifiConnected = true;
        Serial.printf("Connected to AP - Channel: %d, RSSI: %d, IP: ",
                     WiFi.channel(), WiFi.RSSI());
        Serial.println(WiFi.localIP());
    }
}

int8_t findStrongestAP() {
    int8_t strongest_rssi = -127;
    int8_t chosen_channel = -1;
    uint8_t chosen_bssid[6];

    Serial.println("Scanning for networks...");
    int n = WiFi.scanNetworks();

    if (n == 0) {
        Serial.println("No networks found");
        return -127;
    }

    // Find strongest signal matching our SSID
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == String(WIFI_SSID)) {
            if (WiFi.RSSI(i) > strongest_rssi) {
                strongest_rssi = WiFi.RSSI(i);
                chosen_channel = WiFi.channel(i);
                memcpy(chosen_bssid, WiFi.BSSID(i), 6);
            }
        }
        vTaskDelay(10); // Prevent watchdog trigger
    }

    if (chosen_channel != -1) {
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);

        if (!WiFi.config(STATIC_IP, GATEWAY, SUBNET, PRIMARY_DNS, SECONDARY_DNS)) {
            Serial.println("Static IP Configuration Failed");
            return -127;
        }

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD, chosen_channel, chosen_bssid);
        Serial.printf("Selected AP - Channel: %d, RSSI: %d\n", chosen_channel, strongest_rssi);
    }

    WiFi.scanDelete();
    return strongest_rssi;
}

void sendJsonResponse(AsyncWebServerRequest *request, const String& json) {
    request->send(200, "application/json", json);
}

void sendErrorResponse(AsyncWebServerRequest *request, int code, const String& message) {
    String json = "{\"status\":\"error\",\"message\":\"" + message + "\"}";
    request->send(code, "application/json", json);
}