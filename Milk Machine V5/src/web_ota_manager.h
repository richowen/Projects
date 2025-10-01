#ifndef WEB_OTA_MANAGER_H
#define WEB_OTA_MANAGER_H

#include "config.h"
#include "logger.h"
#include <ESPAsyncWebServer.h>
#include <Update.h>

class WebOTAManager {
private:
    Logger& _logger;
    AsyncWebServer _server;
    bool _updateInProgress = false;

    // HTML content for the upload page
    const char* _uploadHtml = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Milk Machine V5 - Firmware Update</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; text-align: center; }
        .status { padding: 10px; margin: 10px 0; border-radius: 4px; }
        .status.idle { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .status.updating { background: #fff3cd; color: #856404; border: 1px solid #ffeaa7; }
        .status.success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .status.error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        form { margin: 20px 0; }
        input[type="file"] { margin: 10px 0; padding: 8px; width: 100%; box-sizing: border-box; }
        input[type="submit"] { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
        input[type="submit"]:hover { background: #0056b3; }
        input[type="submit"]:disabled { background: #6c757d; cursor: not-allowed; }
        .progress { width: 100%; background: #e9ecef; border-radius: 4px; height: 20px; margin: 10px 0; }
        .progress-bar { height: 100%; background: #007bff; border-radius: 4px; transition: width 0.3s; }
        .info { background: #e7f3ff; padding: 15px; border-radius: 4px; margin: 15px 0; }
        .info h3 { margin-top: 0; color: #0066cc; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 Milk Machine V5 Firmware Update</h1>

        <div id="status" class="status idle">Ready for firmware update</div>

        <div class="info">
            <h3>📋 Instructions</h3>
            <ol>
                <li>Select a firmware binary file (.bin)</li>
                <li>Click "Upload Firmware"</li>
                <li>Wait for the update to complete</li>
                <li>Device will automatically reboot</li>
            </ol>
            <p><strong>Note:</strong> Do not disconnect power during update!</p>
        </div>

        <form id="uploadForm" enctype="multipart/form-data" action="/update" method="POST">
            <input type="file" name="firmware" accept=".bin" required>
            <input type="submit" value="Upload Firmware" id="submitBtn">
        </form>

        <div class="progress" style="display: none;">
            <div class="progress-bar" id="progressBar" style="width: 0%;"></div>
        </div>

        <div id="updateInfo"></div>
    </div>

    <script>
        const form = document.getElementById('uploadForm');
        const submitBtn = document.getElementById('submitBtn');
        const statusDiv = document.getElementById('status');
        const progressDiv = document.querySelector('.progress');
        const progressBar = document.getElementById('progressBar');
        const updateInfo = document.getElementById('updateInfo');

        form.addEventListener('submit', function(e) {
            e.preventDefault();

            const formData = new FormData(form);
            const file = formData.get('firmware');

            if (!file || !file.name.endsWith('.bin')) {
                showStatus('Please select a valid .bin file', 'error');
                return;
            }

            submitBtn.disabled = true;
            submitBtn.value = 'Uploading...';
            statusDiv.className = 'status updating';
            statusDiv.textContent = 'Uploading firmware...';
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';

            fetch('/update', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Upload failed: ' + response.status);
                }
                return response.text();
            })
            .then(result => {
                statusDiv.className = 'status success';
                statusDiv.textContent = 'Update successful! Device will reboot...';
                updateInfo.innerHTML = '<p>✅ Firmware uploaded successfully. The device will restart automatically.</p>';
                progressBar.style.width = '100%';
            })
            .catch(error => {
                statusDiv.className = 'status error';
                statusDiv.textContent = 'Update failed: ' + error.message;
                submitBtn.disabled = false;
                submitBtn.value = 'Upload Firmware';
                progressDiv.style.display = 'none';
            });
        });

        function showStatus(message, type) {
            statusDiv.className = 'status ' + type;
            statusDiv.textContent = message;
        }

        // Check for update progress
        setInterval(() => {
            fetch('/progress')
                .then(response => response.json())
                .then(data => {
                    if (data.inProgress) {
                        progressBar.style.width = data.progress + '%';
                        if (data.progress >= 100) {
                            showStatus('Update complete! Rebooting...', 'success');
                        }
                    }
                })
                .catch(err => console.log('Progress check failed'));
        }, 1000);
    </script>
</body>
</html>
)html";

public:
    WebOTAManager(Logger& logger, int port = 80) : _logger(logger), _server(port) {}

    void initialize() {
        _logger.info("Initializing Web OTA Manager...");

        // Main page
        _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
            request->send(200, "text/html", _uploadHtml);
        });

        // Firmware upload handler
        _server.on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
            handleUpdateComplete(request);
        }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleUpdateUpload(request, filename, index, data, len, final);
        });

        // Progress endpoint
        _server.on("/progress", HTTP_GET, [this](AsyncWebServerRequest *request) {
            String json = "{";
            json += "\"inProgress\":" + String(_updateInProgress ? "true" : "false") + ",";
            json += "\"progress\":" + String((Update.progress() * 100) / Update.size());
            json += "}";
            request->send(200, "application/json", json);
        });

        _server.begin();
        _logger.info("Web OTA Manager initialized on port 80");
    }

    void update() {
        // Web server is handled asynchronously
    }

private:
    void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            _logger.info("Starting firmware update: " + filename);

            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
                return request->send(500, "text/plain", "OTA update failed to begin");
            }

            _updateInProgress = true;
        }

        if (Update.write(data, len) != len) {
            Update.printError(Serial);
            _updateInProgress = false;
            return request->send(500, "text/plain", "OTA update failed");
        }

        if (final) {
            if (Update.end(true)) {
                _logger.info("Firmware update successful");
                _updateInProgress = false;
            } else {
                Update.printError(Serial);
                _updateInProgress = false;
                return request->send(500, "text/plain", "OTA update failed to finalize");
            }
        }
    }

    void handleUpdateComplete(AsyncWebServerRequest *request) {
        if (_updateInProgress) {
            request->send(200, "text/plain", "Update in progress...");
        } else {
            request->send(200, "text/plain", "Update complete. Rebooting...");
            delay(1000);
            ESP.restart();
        }
    }
};

#endif // WEB_OTA_MANAGER_H