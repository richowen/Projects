#include "web_server.h"
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// ========================================
// CONSTRUCTOR
// ========================================

WebServerManager::WebServerManager(IConfigManager* config, InputManager* inputManager,
                                    IWiFiManager* wifiManager, ILogger* logger, uint16_t port)
    : _config(config), _inputManager(inputManager), _wifiManager(wifiManager),
      _logger(logger), _server(port) {
}

// ========================================
// PUBLIC METHODS
// ========================================

bool WebServerManager::begin() {
    _logger->info("Web Server: Initializing...");

    if (!LittleFS.begin(true)) {
        _logger->error("Web Server: Failed to mount LittleFS");
        return false;
    }

    // Start mDNS responder so the panel can be reached at controlpanel.local
    if (MDNS.begin("controlpanel")) {
        MDNS.addService("http", "tcp", 80);
        _logger->info("Web Server: mDNS responder started (controlpanel.local)");
    } else {
        _logger->warning("Web Server: mDNS responder failed to start");
    }

    setupRoutes();

    _server.begin();
    _logger->info("Web Server: Started on port 80");
    return true;
}

// ========================================
// PRIVATE METHODS
// ========================================

void WebServerManager::setupRoutes() {
    // API routes must be registered before the static file catch-all below,
    // otherwise serveStatic("/") intercepts /api/* requests first.

    // GET /api/inputs - current config + names for all physical inputs
    _server.on("/api/inputs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetInputs(request);
    });

    // GET /api/status - WiFi/system status
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });

    // POST /api/input - update one input's entity/service/type live
    AsyncCallbackJsonWebHandler* inputHandler = new AsyncCallbackJsonWebHandler(
        "/api/input",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            handlePostInput(request, json);
        }
    );
    _server.addHandler(inputHandler);

    // Serve static UI files from LittleFS (catch-all, must be last)
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 404 handler
    _server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not found");
    });
}


void WebServerManager::handleGetInputs(AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < 10; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["index"] = i;
        obj["name"] = _config->getInputName(i);
        obj["entityId"] = _config->getInputEntityId(i);
        obj["service"] = _config->getInputService(i);
        obj["type"] = _config->getInputType(i); // 0 = momentary, 1 = toggle
        obj["label"] = _config->getInputLabel(i);


        const InputManager::ButtonState* state = _inputManager->getButtonState(i);
        if (state) {
            obj["pin"] = state->pin;
            if (state->type == InputManager::TOGGLE_SWITCH) {
                obj["currentState"] = (state->currentState == LOW);
            } else {
                obj["currentState"] = nullptr;
            }
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["wifiConnected"] = _wifiManager->isConnected();
    doc["ip"] = _wifiManager->getIPAddress();
    doc["signal"] = _wifiManager->getSignalStrength();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handlePostInput(AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject body = json.as<JsonObject>();

    if (!body.containsKey("index") || !body.containsKey("entityId") ||
        !body.containsKey("service") || !body.containsKey("type")) {
        request->send(400, "application/json", "{\"error\":\"Missing required field(s)\"}");
        return;
    }

    int index = body["index"].as<int>();
    const char* entityId = body["entityId"].as<const char*>();
    const char* service = body["service"].as<const char*>();
    int type = body["type"].as<int>();
    const char* label = body.containsKey("label") ? body["label"].as<const char*>() : "";
    if (label == nullptr) label = "";

    if (index < 0 || index >= 10 || type < 0 || type > 1 || entityId == nullptr || service == nullptr) {
        request->send(400, "application/json", "{\"error\":\"Invalid field value(s)\"}");
        return;
    }

    bool saved = _config->setInputConfig((uint8_t)index, entityId, service, (uint8_t)type, label);

    if (!saved) {
        request->send(500, "application/json", "{\"error\":\"Failed to save configuration\"}");
        return;
    }

    _inputManager->reloadInputConfig((uint8_t)index);

    _logger->logf("INFO", "Web Server: Input %d reconfigured - Entity: %s, Service: %s, Type: %d",
                  index, entityId, service, type);

    request->send(200, "application/json", "{\"success\":true}");
}
