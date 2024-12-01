// system_monitor.h
#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include <EEPROM.h>
#include <esp_task_wdt.h>

// EEPROM addresses for different statistics
#define EEPROM_SIZE 512
#define ADDR_TOTAL_RUNTIME 0      // 4 bytes
#define ADDR_TOTAL_MIXES 4        // 4 bytes
#define ADDR_ERROR_COUNT 8        // 4 bytes
#define ADDR_LAST_ERROR 12        // 1 byte
#define ADDR_LAST_UPTIME 13       // 4 bytes
#define ADDR_ERROR_LOG_START 20   // Error log entries start here

// Watchdog timeout in seconds
#define WDT_TIMEOUT 30

class SystemMonitor {
public:
    // Initialize system monitor
    void begin() {
        // Initialize EEPROM
        if (!EEPROM.begin(EEPROM_SIZE)) {
            Serial.println("Failed to initialize EEPROM!");
            return;
        }

        // Initialize watchdog timer
        esp_task_wdt_init(WDT_TIMEOUT, true); // Enable panic so ESP32 restarts
        esp_task_wdt_add(NULL); // Add current thread to WDT watch

        // Load statistics from EEPROM
        loadStats();
        
        // Record start time
        startTime = millis();
        
        // Log system start
        Serial.println("System Monitor initialized");
        logSystemStart();
    }

    // Feed watchdog timer (call this regularly in main loop)
    void feedWatchdog() {
        esp_task_wdt_reset();
    }

    // Record a mixing operation
    void recordMix() {
        totalMixes++;
        if (totalMixes % 10 == 0) { // Save every 10 mixes to reduce wear
            EEPROM.writeUInt(ADDR_TOTAL_MIXES, totalMixes);
            EEPROM.commit();
        }
    }

    // Log an error
    void logError(uint8_t errorCode, const char* errorMsg) {
        errorCount++;
        lastError = errorCode;
        
        // Save error count and code
        EEPROM.writeUInt(ADDR_ERROR_COUNT, errorCount);
        EEPROM.writeByte(ADDR_LAST_ERROR, lastError);
        
        // Calculate position in circular error log
        int logPos = ADDR_ERROR_LOG_START + (errorCount % 20) * 32; // 32 bytes per entry
        
        // Save timestamp and error message
        uint32_t timestamp = millis();
        EEPROM.writeUInt(logPos, timestamp);
        EEPROM.writeString(logPos + 4, errorMsg);
        
        EEPROM.commit();
    }

    // Update runtime statistics (call periodically)
    void updateStats() {
        uint32_t currentTime = millis();
        if (currentTime - lastStatUpdate >= 60000) { // Update every minute
            totalRuntime += (currentTime - lastStatUpdate) / 1000;
            EEPROM.writeUInt(ADDR_TOTAL_RUNTIME, totalRuntime);
            EEPROM.writeUInt(ADDR_LAST_UPTIME, (currentTime - startTime) / 1000);
            EEPROM.commit();
            lastStatUpdate = currentTime;
        }
    }

    // Get total runtime in seconds
    uint32_t getTotalRuntime() {
        return totalRuntime;
    }

    // Get total number of mixes
    uint32_t getTotalMixes() {
        return totalMixes;
    }

    // Get error count
    uint32_t getErrorCount() {
        return errorCount;
    }

    // Get last error code
    uint8_t getLastError() {
        return lastError;
    }

    // Get current uptime in seconds
    uint32_t getCurrentUptime() {
        return (millis() - startTime) / 1000;
    }

private:
    uint32_t startTime = 0;
    uint32_t lastStatUpdate = 0;
    uint32_t totalRuntime = 0;
    uint32_t totalMixes = 0;
    uint32_t errorCount = 0;
    uint8_t lastError = 0;

    // Load statistics from EEPROM
    void loadStats() {
        totalRuntime = EEPROM.readUInt(ADDR_TOTAL_RUNTIME);
        totalMixes = EEPROM.readUInt(ADDR_TOTAL_MIXES);
        errorCount = EEPROM.readUInt(ADDR_ERROR_COUNT);
        lastError = EEPROM.readByte(ADDR_LAST_ERROR);
    }

    // Log system start in error log
    void logSystemStart() {
        logError(0xFF, "System Start"); // Use 0xFF as special code for system start
    }
};

#endif // SYSTEM_MONITOR_H
