#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>

// Alert types enumeration
enum AlertType {
    ALERT_CAR = 0,
    ALERT_TRUCK = 1,
    ALERT_MOTORCYCLE = 2,
    ALERT_PEDESTRIAN = 3,
    ALERT_UNKNOWN = 4,
    ALERT_COUNT = 5
};

// Alert configuration structure
struct AlertConfig {
    String name;
    int ledPattern;        // 0: solid, 1: blink, 2: breathe, 3: strobe
    int ledDuration;       // Duration in milliseconds
    int soundSequence[5];  // Array of frequencies (Hz), 0 = silence
    int soundDurations[5]; // Duration for each sound in ms
    int soundCount;        // Number of sounds in sequence
    int priority;          // 0: low, 1: medium, 2: high
};

// Function prototypes
void initAlerts();
void triggerAlert(AlertType type);
void updateAlerts();
void stopAllAlerts();
bool isAlertActive();
AlertConfig getAlertConfig(AlertType type);
void setAlertConfig(AlertType type, AlertConfig config);
void saveAlertConfigs();
void loadAlertConfigs();

#endif // ALERTS_H