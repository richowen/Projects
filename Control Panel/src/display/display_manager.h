#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include "icons.h"

// ========================================
// DISPLAY MODES
// ========================================

enum DisplayMode {
    MODE_BOOT_SEQUENCE,      // Initial startup animation
    MODE_WIFI_STATUS,        // WiFi connection progress
    MODE_IDLE,               // Standby/ready state
    MODE_IDLE_ANIMATION,     // Screensaver animations
    MODE_ACTION_ICON,        // Show action being performed
    MODE_PROGRESS_BAR,       // Progress indicator
    MODE_SUCCESS,            // Success feedback
    MODE_ERROR,              // Error feedback
    MODE_TEMP_DISPLAY,       // Temperature display
    MODE_STATUS_INFO         // System status
};

// ========================================
// PROGRESS BAR STYLES
// ========================================

enum ProgressStyle {
    PROGRESS_VERTICAL,       // Fill from bottom to top
    PROGRESS_HORIZONTAL,     // Fill from left to right
    PROGRESS_BORDER,         // Draw around perimeter
    PROGRESS_SEGMENTED,      // 8 segments
    PROGRESS_SPIRAL          // Spiral fill from outside to inside (clockwise)
};

// ========================================
// IDLE ANIMATION TYPES
// ========================================

enum IdleAnimationType {
    IDLE_BREATHING,          // Breathing border
    IDLE_MATRIX_RAIN,        // Matrix-style rain
    IDLE_WAVE,               // Sine wave
    IDLE_STARFIELD,          // Random twinkling
    IDLE_SNAKE,              // Snake pattern
    IDLE_COUNT               // Total count
};

// ========================================
// ANIMATION STRUCTURE
// ========================================

struct Animation {
    const uint8_t** frames;     // Pointer to frame array
    uint8_t frameCount;          // Number of frames
    uint16_t frameDelay;         // Milliseconds per frame
    bool loop;                   // Loop animation?
    uint8_t currentFrame;        // Current frame index
    unsigned long lastUpdate;    // Last frame change time
};

// ========================================
// DISPLAY MANAGER CLASS
// ========================================

class DisplayManager {
private:
    MD_Parola* parola;
    MD_MAX72XX* mx;
    
    DisplayMode currentMode;
    DisplayMode previousMode;
    
    unsigned long modeStartTime;
    unsigned long lastUpdate;
    unsigned long idleStartTime;
    
    // Animation state
    uint8_t currentAnimationFrame;
    unsigned long lastAnimationUpdate;
    
    // Idle animation
    IdleAnimationType currentIdleAnimation;
    unsigned long idleAnimationStartTime;
    
    // Status indicators
    bool statusBarEnabled;
    uint8_t wifiSignalLevel;    // 0-3
    bool haConnected;
    
    // Helper functions
    void rotateDisplayCCW();
    void drawIcon(uint8_t iconIndex);
    void drawIconRaw(const uint8_t* iconData);
    void updateIdleAnimation();
    void updateSpinnerAnimation();
    
    // Idle animation implementations
    void animateBreathing();
    void animateMatrixRain();
    void animateWave();
    void animateStarfield();
    void animateSnake();
    
    // Boot sequence
    uint8_t bootSequenceStep;
    void updateBootSequence();
    
    // Matrix rain state
    struct RainColumn {
        int8_t position;
        uint8_t speed;
        uint8_t length;
    };
    RainColumn rainColumns[8];
    
    // Starfield state
    struct Star {
        uint8_t x, y;
        uint8_t brightness;
        int8_t twinkleDirection;
    };
    Star stars[8];
    
    // Snake state
    struct SnakeSegment {
        uint8_t x, y;
    };
    SnakeSegment snake[16];
    uint8_t snakeLength;
    uint8_t snakeDirection;  // 0=up, 1=right, 2=down, 3=left
    
    // Wave state
    float wavePhase;

public:
    DisplayManager();
    
    // Initialization
    void init(MD_Parola* display);
    
    // Main update loop (call every loop iteration)
    void update();
    
    // Mode control
    void setMode(DisplayMode mode);
    DisplayMode getMode() { return currentMode; }
    
    // Icon display
    void showIcon(uint8_t iconIndex, uint16_t duration = 1000);
    void showActionIcon(const char* actionName);
    
    // Feedback
    void showSuccess(uint16_t duration = 1000);
    void showError(uint16_t duration = 1500);
    void showSpinner();
    
    // Progress
    void showProgress(uint8_t percent, ProgressStyle style = PROGRESS_VERTICAL);
    void updateProgress(uint8_t percent);
    
    // Temperature
    void showTemperature(int temp);
    
    // Boot sequence
    void showBootSequence();
    
    // Idle animations
    void setIdleAnimation(IdleAnimationType type);
    void enableIdleAnimations(bool enable);
    
    // Status indicators
    void setWiFiSignal(uint8_t level);  // 0-3
    void setHAConnection(bool connected);
    void showStatusBar(bool show);
    
    // Utility
    void clear();
    void flash(uint16_t duration = 100);
};

#endif // DISPLAY_MANAGER_H