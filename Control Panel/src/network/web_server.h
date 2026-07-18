#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "interfaces.h"
#include "io/input_manager.h"

/**
 * @brief Web server for live control panel configuration
 *
 * Serves the graphical web UI from LittleFS and exposes a JSON REST API
 * to view and live-edit the function (entity/service/type) of each
 * physical button/switch without recompiling or reflashing firmware.
 */
class WebServerManager {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager reference
     * @param inputManager Input manager reference (concrete type, needed for live reload)
     * @param wifiManager WiFi manager reference
     * @param logger Logger reference
     * @param port HTTP port (default 80)
     */
    WebServerManager(IConfigManager* config, InputManager* inputManager,
                      IWiFiManager* wifiManager, ILogger* logger, uint16_t port = 80);

    /**
     * @brief Initialize LittleFS, mDNS and start the web server
     * @return true if initialized successfully
     */
    bool begin();

private:
    IConfigManager* _config;
    InputManager* _inputManager;
    IWiFiManager* _wifiManager;
    ILogger* _logger;
    AsyncWebServer _server;

    void setupRoutes();
    void handleGetInputs(AsyncWebServerRequest* request);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handlePostInput(AsyncWebServerRequest* request, JsonVariant& json);
};

#endif // WEB_SERVER_H
