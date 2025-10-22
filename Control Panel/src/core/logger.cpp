#include "logger.h"
#include <stdarg.h>

// ========================================
// CONSTRUCTOR
// ========================================

Logger::Logger(LogLevel minLevel) : _minLevel(minLevel) {
    // Initialize serial for logging if not already done
    if (!Serial) {
        Serial.begin(115200);
        delay(100); // Give serial time to initialize
    }
}

// ========================================
// PUBLIC METHODS
// ========================================

void Logger::setLogLevel(LogLevel level) {
    _minLevel = level;
}

void Logger::debug(const char* message) {
    log(DEBUG, message);
}

void Logger::info(const char* message) {
    log(INFO, message);
}

void Logger::warning(const char* message) {
    log(WARNING, message);
}

void Logger::error(const char* message) {
    log(ERROR, message);
}

void Logger::logf(const char* levelStr, const char* format, ...) {
    // Convert level string to enum
    LogLevel level = INFO; // default
    if (strcmp(levelStr, "DEBUG") == 0) level = DEBUG;
    else if (strcmp(levelStr, "INFO") == 0) level = INFO;
    else if (strcmp(levelStr, "WARNING") == 0) level = WARNING;
    else if (strcmp(levelStr, "ERROR") == 0) level = ERROR;

    // Only log if level meets minimum threshold
    if (level < _minLevel) return;

    // Format the message
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Log with timestamp and level
    Serial.print("[");
    Serial.print(getTimestamp());
    Serial.print("] [");
    Serial.print(levelToString(level));
    Serial.print("] ");
    Serial.println(buffer);
}

// ========================================
// PRIVATE METHODS
// ========================================

String Logger::getTimestamp() const {
    unsigned long now = millis();
    unsigned long seconds = now / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    char buffer[9];
    sprintf(buffer, "%02lu:%02lu:%02lu",
            hours % 24,
            minutes % 60,
            seconds % 60);

    return String(buffer);
}

void Logger::log(LogLevel level, const char* message) {
    // Only log if level meets minimum threshold
    if (level < _minLevel) return;

    Serial.print("[");
    Serial.print(getTimestamp());
    Serial.print("] [");
    Serial.print(levelToString(level));
    Serial.print("] ");
    Serial.println(message);
}

const char* Logger::levelToString(LogLevel level) const {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO";
        case WARNING: return "WARN";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}