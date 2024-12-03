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
#define ADDR_VALIDATION_BYTE 510  // Validation byte to check if EEPROM is initialized

// Watchdog timeout in seconds
#define WDT_TIMEOUT 30

// EEPROM validation value
#define EEPROM_VALID_VALUE 0xAA

class SystemMonitor {
public:
    // Initialize system monitor
    bool begin() {
        // Initialize EEPROM
        if (!EEPROM.begin(EEPROM_SIZE)) {
            Serial.println("Failed to initialize EEPROM!");
            return false;
        }

        // Check if EEPROM needs initialization
        if (EEPROM.read(ADDR_VALIDATION_BYTE) != EEPROM_VALID_VALUE) {
            initializeEEPROM();
        }

        // Initialize watchdog timer
        esp_err_t err = esp_task_wdt_init(WDT_TIMEOUT, true); // Enable panic so ESP32 restarts
        if (err != ESP_OK) {
            Serial.println("Failed to initialize watchdog!");
            return false;
        }

        err = esp_task_wdt_add(NULL); // Add current thread to WDT watch
        if (err != ESP_OK) {
            Serial.println("Failed to add task to watchdog!");
            return false;
        }

        // Load statistics from EEPROM
        loadStats();
        
        // Record start time
        startTime = millis();
        
        // Log system start
        Serial.println("System Monitor initialized");
        logSystemStart();

        return true;
    }

    // Feed watchdog timer (call this regularly in main loop)
    void feedWatchdog() {
        esp_task_wdt_reset();
    }

    // Record a mixing operation
    void recordMix() {
        totalMixes++;
        if (totalMixes % 10 == 0) { // Save every 10 mixes to reduce wear
            writeWithRetry(ADDR_TOTAL_MIXES, totalMixes);
        }
    }

    // Log an error
    void logError(uint8_t errorCode, const char* errorMsg) {
        errorCount++;
        lastError = errorCode;
        
        // Save error count and code with retry
        writeWithRetry(ADDR_ERROR_COUNT, errorCount);
        writeWithRetry(ADDR_LAST_ERROR, lastError);
        
        // Calculate position in circular error log
        int logPos = ADDR_ERROR_LOG_START + (errorCount % 20) * 32; // 32 bytes per entry
        
        // Save timestamp and error message
        uint32_t timestamp = millis();
        writeWithRetry(logPos, timestamp);
        writeStringWithRetry(logPos + 4, errorMsg);
    }

    // Update runtime statistics (call periodically)
    void updateStats() {
        uint32_t currentTime = millis();
        if ((long)(currentTime - lastStatUpdate) >= 60000) { // Update every minute
            totalRuntime += ((long)(currentTime - lastStatUpdate)) / 1000;
            writeWithRetry(ADDR_TOTAL_RUNTIME, totalRuntime);
            writeWithRetry(ADDR_LAST_UPTIME, (currentTime - startTime) / 1000);
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
        return ((long)(millis() - startTime)) / 1000;
    }

private:
    uint32_t startTime = 0;
    uint32_t lastStatUpdate = 0;
    uint32_t totalRuntime = 0;
    uint32_t totalMixes = 0;
    uint32_t errorCount = 0;
    uint8_t lastError = 0;

    // Initialize EEPROM with default values
    void initializeEEPROM() {
        writeWithRetry(ADDR_TOTAL_RUNTIME, 0U);
        writeWithRetry(ADDR_TOTAL_MIXES, 0U);
        writeWithRetry(ADDR_ERROR_COUNT, 0U);
        writeWithRetry(ADDR_LAST_ERROR, 0U);
        writeWithRetry(ADDR_LAST_UPTIME, 0U);
        
        // Write validation byte
        EEPROM.write(ADDR_VALIDATION_BYTE, EEPROM_VALID_VALUE);
        EEPROM.commit();
    }

    // Load statistics from EEPROM
    void loadStats() {
        totalRuntime = EEPROM.readUInt(ADDR_TOTAL_RUNTIME);
        totalMixes = EEPROM.readUInt(ADDR_TOTAL_MIXES);
        errorCount = EEPROM.readUInt(ADDR_ERROR_COUNT);
        lastError = EEPROM.readByte(ADDR_LAST_ERROR);

        // Validate loaded values
        if (totalRuntime == 0xFFFFFFFF) totalRuntime = 0;
        if (totalMixes == 0xFFFFFFFF) totalMixes = 0;
        if (errorCount == 0xFFFFFFFF) errorCount = 0;
        if (lastError == 0xFF) lastError = 0;
    }

    // Write to EEPROM with retry
    template<typename T>
    void writeWithRetry(int addr, T value, int maxRetries = 3) {
        for (int i = 0; i < maxRetries; i++) {
            if (sizeof(T) == 1) {
                EEPROM.write(addr, value);
            } else {
                EEPROM.put(addr, value);
            }
            
            if (EEPROM.commit()) {
                // Verify write
                T readValue;
                if (sizeof(T) == 1) {
                    readValue = EEPROM.read(addr);
                } else {
                    EEPROM.get(addr, readValue);
                }
                
                if (readValue == value) {
                    return;
                }
            }
            delay(10);  // Wait before retry
        }
        Serial.println("EEPROM write failed after retries");
    }

    // Write string to EEPROM with retry
    void writeStringWithRetry(int addr, const char* str, int maxRetries = 3) {
        for (int i = 0; i < maxRetries; i++) {
            EEPROM.writeString(addr, str);
            if (EEPROM.commit()) {
                String readValue = EEPROM.readString(addr);
                if (readValue == str) {
                    return;
                }
            }
            delay(10);  // Wait before retry
        }
        Serial.println("EEPROM string write failed after retries");
    }

    // Log system start in error log
    void logSystemStart() {
        logError(0xFF, "System Start"); // Use 0xFF as special code for system start
    }
};

#endif // SYSTEM_MONITOR_H
