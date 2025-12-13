#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <esp_task_wdt.h>

// Function prototypes
void initSystem();
void checkSystemHealth();
void logSystemEvent(const String& event);
void logError(const String& error);
void resetWatchdog();
String getSystemStatusJson();

#endif // SYSTEM_H