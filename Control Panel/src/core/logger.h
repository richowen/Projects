#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "interfaces.h"

/**
 * @brief Simple serial logger implementation
 *
 * Provides structured logging with different levels and timestamps.
 * In production, this could be extended to log to SD card, network, etc.
 */
class Logger : public ILogger {
public:
    /**
     * @brief Log levels for filtering
     */
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };

    /**
     * @brief Constructor
     * @param minLevel Minimum log level to output (default: INFO)
     */
    Logger(LogLevel minLevel = INFO);

    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    void setLogLevel(LogLevel level);

    // ILogger interface implementation
    void debug(const char* message) override;
    void info(const char* message) override;
    void warning(const char* message) override;
    void error(const char* message) override;
    void logf(const char* level, const char* format, ...) override;

private:
    LogLevel _minLevel;

    /**
     * @brief Get current timestamp string
     * @return timestamp in HH:MM:SS format
     */
    String getTimestamp() const;

    /**
     * @brief Internal logging function
     * @param level Log level
     * @param message Message to log
     */
    void log(LogLevel level, const char* message);

    /**
     * @brief Convert log level to string
     * @param level Log level
     * @return level string
     */
    const char* levelToString(LogLevel level) const;
};

#endif // LOGGER_H