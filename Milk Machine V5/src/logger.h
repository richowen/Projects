#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <Print.h>
#include <memory>
#include <vector>

// Forward declaration
class Logger;

// Custom stream class for dual output
class DualPrint : public Print {
private:
    WiFiClient* _client = nullptr;
    Logger* _logger = nullptr;

public:
    void setClient(WiFiClient* client) { _client = client; }
    void setLogger(Logger* logger) { _logger = logger; }

    size_t write(uint8_t c) override {
        if (_client && _client->connected()) {
            _client->write(c);
        }
        return Serial.write(c);
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        if (_client && _client->connected()) {
            _client->write(buffer, size);
        }
        return Serial.write(buffer, size);
    }
};

// Observer pattern for log subscribers
class LogObserver {
public:
    virtual ~LogObserver() = default;
    virtual void onLogMessage(const String& message) = 0;
};

// Main Logger class
class Logger {
private:
    DualPrint _dualPrint;
    std::vector<LogObserver*> _observers;

public:
    Logger() {
        _dualPrint.setLogger(this);
    }

    void addObserver(LogObserver* observer) {
        _observers.push_back(observer);
    }

    void removeObserver(LogObserver* observer) {
        _observers.erase(
            std::remove(_observers.begin(), _observers.end(), observer),
            _observers.end()
        );
    }

    void setTelnetClient(WiFiClient* client) {
        _dualPrint.setClient(client);
    }

    // Logging methods
    void info(const String& message) {
        log("INFO", message);
    }

    void warning(const String& message) {
        log("WARN", message);
    }

    void error(const String& message) {
        log("ERROR", message);
    }

    void debug(const String& message) {
        log("DEBUG", message);
    }

    void printf(const char* format, ...) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        _dualPrint.print(buffer);
        notifyObservers(String(buffer));
    }

    Print& getPrint() {
        return _dualPrint;
    }

private:
    void log(const char* level, const String& message) {
        char timestamp[20];
        unsigned long seconds = millis() / 1000;
        snprintf(timestamp, sizeof(timestamp), "[%lu] ", seconds);

        String fullMessage = String(timestamp) + "[" + level + "] " + message + "\n";
        _dualPrint.print(fullMessage);
        notifyObservers(fullMessage);
    }

    void notifyObservers(const String& message) {
        for (auto observer : _observers) {
            if (observer) {
                observer->onLogMessage(message);
            }
        }
    }
};

#endif // LOGGER_H