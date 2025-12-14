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
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .alert-config {
            border: 1px solid #ccc;
            padding: 15px;
            margin: 10px 0;
            background: white;
            border-radius: 5px;
        }
        input, select { margin: 5px; padding: 5px; }
        button { padding: 10px 20px; margin: 5px; cursor: pointer; }
        .save-btn { background: #4CAF50; color: white; border: none; }
        .test-btn { background: #2196F3; color: white; border: none; }
        .test-btn:hover { background: #0b7dda; }
        .save-btn:hover { background: #45a049; }
    </style>
</head>
<body>
    <h1>CCTV Alert System Configuration</h1>
    <form id="configForm">
        <div id="alerts"></div>
        <button type="submit" class="save-btn">Save Configuration</button>
    </form>
    <script>
        async function loadConfig() {
            const response = await fetch('/api/config');
            const configs = await response.json();
            const container = document.getElementById('alerts');

            configs.forEach((config, index) => {
                const div = document.createElement('div');
                div.className = 'alert-config';
                
                let soundsHTML = '';
                for (let i = 0; i < 5; i++) {
                    const freq = config.soundSequence[i] || 0;
                    const dur = config.soundDurations[i] || 0;
                    soundsHTML += `
                        <div style="display: inline-block; margin-right: 10px;">
                            <label>Tone ${i+1} Freq (Hz): <input type="number" name="sound_${index}_${i}" value="${freq}" style="width: 80px;"></label>
                            <label>Duration (ms): <input type="number" name="dur_${index}_${i}" value="${dur}" style="width: 70px;"></label>
                        </div>
                    `;
                }
                
                const alertTypes = ['car', 'truck', 'motorcycle', 'pedestrian', 'unknown'];
                
                div.innerHTML = `
                    <div style="display: flex; justify-content: space-between; align-items: center;">
                        <h3 style="margin: 0;">${config.name}</h3>
                        <button type="button" class="test-btn" onclick="testAlert('${alertTypes[index]}')">Test Alert</button>
                    </div>
                    <hr style="margin: 10px 0;">
                    <label>LED Pattern:
                        <select name="pattern_${index}">
                            <option value="0" ${config.ledPattern == 0 ? 'selected' : ''}>Solid</option>
                            <option value="1" ${config.ledPattern == 1 ? 'selected' : ''}>Blink</option>
                            <option value="2" ${config.ledPattern == 2 ? 'selected' : ''}>Breathe</option>
                            <option value="3" ${config.ledPattern == 3 ? 'selected' : ''}>Strobe</option>
                        </select>
                    </label>
                    <label>LED Duration (ms): <input type="number" name="duration_${index}" value="${config.ledDuration}"></label>
                    <label>Priority:
                        <select name="priority_${index}">
                            <option value="0" ${config.priority == 0 ? 'selected' : ''}>Low</option>
                            <option value="1" ${config.priority == 1 ? 'selected' : ''}>Medium</option>
                            <option value="2" ${config.priority == 2 ? 'selected' : ''}>High</option>
                        </select>
                    </label>
                    <div style="margin-top: 10px;">
                        <strong>Sound Sequence (use 0 for silence):</strong><br>
                        ${soundsHTML}
                    </div>
                `;
                container.appendChild(div);
            });
        }

        async function testAlert(type) {
            try {
                const response = await fetch(`/test?type=${type}`);
                const result = await response.json();
                console.log('Test result:', result);
            } catch (error) {
                console.error('Test failed:', error);
                alert('Failed to test alert: ' + error.message);
            }
        }

        document.getElementById('configForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const response = await fetch('/api/config', {
                method: 'POST',
                body: formData
            });
            const result = await response.text();
            alert(result);
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
        // Check if parameter exists before accessing (false = GET query parameter)
        if (!request->hasParam("type")) {
            sendErrorResponse(request, 400, "Missing 'type' parameter");
            return;
        }
        
        String typeParam = request->getParam("type")->value();
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
            json += "\"priority\":" + String(config.priority) + ",";
            json += "\"soundSequence\":[";
            for (int j = 0; j < 5; j++) {
                if (j > 0) json += ",";
                json += String(config.soundSequence[j]);
            }
            json += "],";
            json += "\"soundDurations\":[";
            for (int j = 0; j < 5; j++) {
                if (j > 0) json += ",";
                json += String(config.soundDurations[j]);
            }
            json += "]";
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
            } else if (name.startsWith("sound_")) {
                // Format: sound_<alertIndex>_<soundIndex>
                int firstUnderscore = name.indexOf('_', 6);
                if (firstUnderscore > 0) {
                    int alertIndex = name.substring(6, firstUnderscore).toInt();
                    int soundIndex = name.substring(firstUnderscore + 1).toInt();
                    if (alertIndex >= 0 && alertIndex < ALERT_COUNT && soundIndex >= 0 && soundIndex < 5) {
                        AlertConfig config = getAlertConfig((AlertType)alertIndex);
                        config.soundSequence[soundIndex] = value.toInt();
                        // Update sound count based on non-zero frequencies
                        int count = 0;
                        for (int j = 0; j < 5; j++) {
                            if (config.soundSequence[j] > 0) count = j + 1;
                        }
                        config.soundCount = count;
                        setAlertConfig((AlertType)alertIndex, config);
                    }
                }
            } else if (name.startsWith("dur_")) {
                // Format: dur_<alertIndex>_<soundIndex>
                int firstUnderscore = name.indexOf('_', 4);
                if (firstUnderscore > 0) {
                    int alertIndex = name.substring(4, firstUnderscore).toInt();
                    int soundIndex = name.substring(firstUnderscore + 1).toInt();
                    if (alertIndex >= 0 && alertIndex < ALERT_COUNT && soundIndex >= 0 && soundIndex < 5) {
                        AlertConfig config = getAlertConfig((AlertType)alertIndex);
                        config.soundDurations[soundIndex] = value.toInt();
                        setAlertConfig((AlertType)alertIndex, config);
                    }
                }
            }
        }

        saveAlertConfigs();
        sendJsonResponse(request, "{\"status\":\"success\",\"message\":\"Configuration saved\"}");
    });

    // Test endpoints
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Check if parameter exists before accessing (false = GET query parameter)
        if (!request->hasParam("type")) {
            sendErrorResponse(request, 400, "Missing 'type' parameter");
            return;
        }
        
        String typeParam = request->getParam("type")->value();
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
    
    // Catch-all handler for unknown requests (prevents crashes from scanners/browsers)
    server.onNotFound([](AsyncWebServerRequest *request) {
        sendErrorResponse(request, 404, "Endpoint not found");
    });
}


void connectToWifi() {
    // Check if already connected
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        return;
    }

    unsigned long currentMillis = millis();
    
    // Throttle reconnection attempts
    if (currentMillis - lastReconnectAttempt < currentReconnectInterval) {
        return;
    }
    
    lastReconnectAttempt = currentMillis;
    Serial.println("Connecting to WiFi with static IP...");
    
    // Configure static IP before connecting
    if (!WiFi.config(STATIC_IP, GATEWAY, SUBNET, PRIMARY_DNS, SECONDARY_DNS)) {
        Serial.println("[ERROR] Failed to configure static IP");
    }
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait for connection with timeout
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_SCAN_TIMEOUT) {
        resetWatchdog();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        currentReconnectInterval = MIN_RECONNECT_INTERVAL;
        Serial.printf("Connected! IP: %s, RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        wifiConnected = false;
        currentReconnectInterval = min(currentReconnectInterval * 2, MAX_RECONNECT_INTERVAL);
        Serial.printf("Connection failed. Retry in %d seconds\n", currentReconnectInterval / 1000);
    }
}

int8_t findStrongestAP() {
    // This function is no longer used but kept for compatibility
    return WiFi.RSSI();
}

void sendJsonResponse(AsyncWebServerRequest *request, const String& json) {
    request->send(200, "application/json", json);
}

void sendErrorResponse(AsyncWebServerRequest *request, int code, const String& message) {
    String json = "{\"status\":\"error\",\"message\":\"" + message + "\"}";
    request->send(code, "application/json", json);
}