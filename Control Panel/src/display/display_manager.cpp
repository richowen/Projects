#include "display_manager.h"
#include <Arduino.h>
#include "config.h"

// Configuration from main config
#define DISPLAY_HEIGHT 8
#define DISPLAY_IDLE_TIMEOUT 30000
#define DISPLAY_IDLE_ANIMATION_DURATION 10000

// ========================================
// SPIRAL PROGRESS PATH
// ========================================
// Pre-computed spiral path for 8x8 matrix (clockwise, outside-to-inside)
// Each entry is {row, col} representing the LED position in spiral order
// Total: 64 pixels (28 + 20 + 12 + 4 from 4 concentric rings)
struct SpiralCoord {
    uint8_t row;
    uint8_t col;
};

const SpiralCoord SPIRAL_PATH[64] PROGMEM = {
    // Ring 0 (outer 8x8) - 28 pixels
    // Top edge (left to right)
    {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7},
    // Right edge (top to bottom, excluding corner)
    {1,7}, {2,7}, {3,7}, {4,7}, {5,7}, {6,7}, {7,7},
    // Bottom edge (right to left, excluding corner)
    {7,6}, {7,5}, {7,4}, {7,3}, {7,2}, {7,1}, {7,0},
    // Left edge (bottom to top, excluding corners)
    {6,0}, {5,0}, {4,0}, {3,0}, {2,0}, {1,0},
    
    // Ring 1 (6x6) - 20 pixels
    // Top edge (left to right)
    {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6},
    // Right edge (top to bottom, excluding corner)
    {2,6}, {3,6}, {4,6}, {5,6}, {6,6},
    // Bottom edge (right to left, excluding corner)
    {6,5}, {6,4}, {6,3}, {6,2}, {6,1},
    // Left edge (bottom to top, excluding corners)
    {5,1}, {4,1}, {3,1}, {2,1},
    
    // Ring 2 (4x4) - 12 pixels
    // Top edge (left to right)
    {2,2}, {2,3}, {2,4}, {2,5},
    // Right edge (top to bottom, excluding corner)
    {3,5}, {4,5}, {5,5},
    // Bottom edge (right to left, excluding corner)
    {5,4}, {5,3}, {5,2},
    // Left edge (bottom to top, excluding corners)
    {4,2}, {3,2},
    
    // Ring 3 (center 2x2) - 4 pixels
    // Top edge (left to right)
    {3,3}, {3,4},
    // Right edge (top to bottom, excluding corner)
    {4,4},
    // Bottom edge (right to left, excluding corner)
    {4,3}
};

// ========================================
// CONSTRUCTOR
// ========================================

DisplayManager::DisplayManager() 
    : parola(nullptr), mx(nullptr), currentMode(MODE_IDLE), previousMode(MODE_IDLE),
      modeStartTime(0), lastUpdate(0), idleStartTime(0),
      currentAnimationFrame(0), lastAnimationUpdate(0),
      currentIdleAnimation(IDLE_MATRIX_RAIN), idleAnimationStartTime(0),
      statusBarEnabled(false), wifiSignalLevel(0), haConnected(false),
      bootSequenceStep(0), wavePhase(0), snakeLength(0), snakeDirection(0) {
}

// ========================================
// INITIALIZATION
// ========================================

void DisplayManager::init(MD_Parola* display) {
    parola = display;
    mx = parola->getGraphicObject();
    
    // Initialize rain columns for matrix rain
    for (int i = 0; i < 8; i++) {
        rainColumns[i].position = random(-8, 0);
        rainColumns[i].speed = random(1, 4);
        rainColumns[i].length = random(2, 5);
    }
    
    // Initialize stars for starfield
    for (int i = 0; i < 8; i++) {
        stars[i].x = random(8);
        stars[i].y = random(8);
        stars[i].brightness = random(2);
        stars[i].twinkleDirection = random(2) ? 1 : -1;
    }
    
    // Initialize snake
    snakeLength = 4;
    for (int i = 0; i < snakeLength; i++) {
        snake[i].x = 4;
        snake[i].y = 4 - i;
    }
    snakeDirection = 0; // up
    
    currentMode = MODE_IDLE;
    modeStartTime = millis();
    idleStartTime = millis();
}

// ========================================
// MAIN UPDATE LOOP
// ========================================

void DisplayManager::update() {
    unsigned long now = millis();
    
    // Check for idle timeout (except during boot or idle animations)
    // Only activate idle animations if they're enabled
    #if ENABLE_IDLE_ANIMATIONS
    if (currentMode != MODE_BOOT_SEQUENCE &&
        currentMode != MODE_IDLE_ANIMATION &&
        (now - idleStartTime) > DISPLAY_IDLE_TIMEOUT) {
        setMode(MODE_IDLE_ANIMATION);
    }
    #endif
    
    // Update based on current mode
    switch (currentMode) {
        case MODE_BOOT_SEQUENCE:
            updateBootSequence();
            break;
            
        case MODE_IDLE_ANIMATION:
            updateIdleAnimation();
            break;
            
        case MODE_ACTION_ICON:
            // Action icons show for 500ms
            if ((now - modeStartTime) > 500) {
                setMode(MODE_IDLE);
            }
            break;
            
        case MODE_SUCCESS:
        case MODE_ERROR:
        case MODE_TEMP_DISPLAY:
            // These modes have fixed durations, check for timeout
            if ((now - modeStartTime) > 3000) {  // Default 3 second timeout
                setMode(MODE_IDLE);
            }
            break;
            
        case MODE_PROGRESS_BAR:
            // Progress bar stays until explicitly changed
            break;
            
        default:
            break;
    }
    
    lastUpdate = now;
}

// ========================================
// MODE CONTROL
// ========================================

void DisplayManager::setMode(DisplayMode mode) {
    if (mode == currentMode) return;
    
    previousMode = currentMode;
    currentMode = mode;
    modeStartTime = millis();
    
    // Reset idle timer on any mode change (except to idle animation)
    if (mode != MODE_IDLE_ANIMATION) {
        idleStartTime = millis();
    }
    
    // Mode-specific initialization
    switch (mode) {
        case MODE_IDLE_ANIMATION:
            idleAnimationStartTime = millis();
            currentAnimationFrame = 0;
            break;
            
        case MODE_BOOT_SEQUENCE:
            bootSequenceStep = 0;
            break;
            
        case MODE_IDLE:
            clear();
            break;
            
        default:
            break;
    }
}

// ========================================
// ROTATION HELPER
// ========================================

void DisplayManager::rotateDisplayCCW() {
    // Rotate display 90° counterclockwise (3 clockwise rotations)
    for (int i = 0; i < 3; i++) {
        mx->transform(MD_MAX72XX::TRC);
    }
    mx->update();
}

// ========================================
// ICON DRAWING
// ========================================

void DisplayManager::drawIcon(uint8_t iconIndex) {
    mx->clear();
    
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t rowData = getIconByte(iconIndex, row);
        for (uint8_t col = 0; col < 8; col++) {
            bool pixel = (rowData >> (7 - col)) & 1;
            mx->setPoint(row, col, pixel);
        }
    }
    
    rotateDisplayCCW();
}

void DisplayManager::drawIconRaw(const uint8_t* iconData) {
    if (!iconData) return;
    
    mx->clear();
    
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t rowData = pgm_read_byte(&iconData[row]);
        for (uint8_t col = 0; col < 8; col++) {
            bool pixel = (rowData >> (7 - col)) & 1;
            mx->setPoint(row, col, pixel);
        }
    }
    
    rotateDisplayCCW();
}

// ========================================
// ICON DISPLAY
// ========================================

void DisplayManager::showIcon(uint8_t iconIndex, uint16_t duration) {
    setMode(MODE_ACTION_ICON);
    drawIcon(iconIndex);
    // Duration handled by update() checking modeStartTime
}

void DisplayManager::showActionIcon(const char* actionName) {
    // Map action names to icons
    uint8_t iconIndex = ICON_POWER;  // Default
    
    if (strstr(actionName, "AC") || strstr(actionName, "Air")) {
        iconIndex = ICON_AC;
    } else if (strstr(actionName, "Light")) {
        iconIndex = ICON_LIGHT;
    } else if (strstr(actionName, "PC") || strstr(actionName, "Shutdown")) {
        iconIndex = ICON_SHUTDOWN;
    } else if (strstr(actionName, "Immersion")) {
        iconIndex = ICON_IMMERSION;
    } else if (strstr(actionName, "Plex")) {
        iconIndex = ICON_PLAY;
    }
    
    showIcon(iconIndex, 500);
}

// ========================================
// FEEDBACK
// ========================================

void DisplayManager::showSuccess(uint16_t duration) {
    setMode(MODE_SUCCESS);
    drawIcon(ICON_CHECK);
}

void DisplayManager::showError(uint16_t duration) {
    setMode(MODE_ERROR);
    drawIcon(ICON_X);
}

void DisplayManager::showSpinner() {
    updateSpinnerAnimation();
}

void DisplayManager::updateSpinnerAnimation() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate > 100) {  // 100ms per frame
        drawIcon(ICON_SPINNER_0 + currentAnimationFrame);
        currentAnimationFrame = (currentAnimationFrame + 1) % 8;
        lastAnimationUpdate = now;
    }
}

// ========================================
// PROGRESS BARS
// ========================================

void DisplayManager::showProgress(uint8_t percent, ProgressStyle style) {
    setMode(MODE_PROGRESS_BAR);
    
    if (percent > 100) percent = 100;
    
    mx->clear();
    
    switch (style) {
        case PROGRESS_VERTICAL:
            // Vertical progress - fill from bottom to top
            {
                int filledRows = (percent * 8) / 100;
                for (int row = 0; row < filledRows; row++) {
                    for (int col = 0; col < 8; col++) {
                        mx->setPoint(row, col, true);
                    }
                }
            }
            break;
            
        case PROGRESS_HORIZONTAL:
            // Horizontal progress - fill from left to right
            {
                int filledCols = (percent * 8) / 100;
                for (int col = 0; col < filledCols; col++) {
                    for (int row = 0; row < 8; row++) {
                        mx->setPoint(row, col, true);
                    }
                }
            }
            break;
            
        case PROGRESS_BORDER:
            // Border progress - draw around perimeter
            {
                int totalPixels = (percent * 28) / 100;  // 28 pixels on border
                int drawn = 0;
                
                // Top edge (left to right)
                for (int col = 0; col < 8 && drawn < totalPixels; col++, drawn++) {
                    mx->setPoint(0, col, true);
                }
                // Right edge (top to bottom)
                for (int row = 1; row < 7 && drawn < totalPixels; row++, drawn++) {
                    mx->setPoint(row, 7, true);
                }
                // Bottom edge (right to left)
                for (int col = 7; col >= 0 && drawn < totalPixels; col--, drawn++) {
                    mx->setPoint(7, col, true);
                }
                // Left edge (bottom to top)
                for (int row = 6; row >= 1 && drawn < totalPixels; row--, drawn++) {
                    mx->setPoint(row, 0, true);
                }
            }
            break;
            
        case PROGRESS_SEGMENTED:
            // Segmented progress - 8 segments
            {
                int filledSegments = (percent * 8) / 100;
                for (int seg = 0; seg < filledSegments; seg++) {
                    mx->setPoint(seg, seg, true);
                }
            }
            break;
            
        case PROGRESS_SPIRAL:
            // Spiral progress - fill from outside to inside (clockwise)
            {
                // Calculate number of pixels to light (0-64)
                int pixelsToLight = (percent * 64) / 100;
                if (pixelsToLight > 64) pixelsToLight = 64;
                
                // Light up pixels along spiral path
                for (int i = 0; i < pixelsToLight; i++) {
                    uint8_t row = pgm_read_byte(&SPIRAL_PATH[i].row);
                    uint8_t col = pgm_read_byte(&SPIRAL_PATH[i].col);
                    mx->setPoint(row, col, true);
                }
            }
            break;
    }
    
    mx->update();
}

void DisplayManager::updateProgress(uint8_t percent) {
    // Legacy function - defaults to vertical
    showProgress(percent, PROGRESS_VERTICAL);
}

// ========================================
// TEMPERATURE DISPLAY
// ========================================

void DisplayManager::showTemperature(int temp) {
    setMode(MODE_TEMP_DISPLAY);
    
    char tempStr[4];
    sprintf(tempStr, "%d", temp);
    
    parola->displayClear();
    parola->setTextAlignment(PA_LEFT);
    parola->print(tempStr);
    
    rotateDisplayCCW();
}

// ========================================
// BOOT SEQUENCE
// ========================================

void DisplayManager::showBootSequence() {
    setMode(MODE_BOOT_SEQUENCE);
    bootSequenceStep = 0;
}

void DisplayManager::updateBootSequence() {
    unsigned long now = millis();
    unsigned long elapsed = now - modeStartTime;
    
    mx->clear();
    
    // Step 0: Center pulse (0-300ms)
    if (bootSequenceStep == 0) {
        if (elapsed < 300) {
            mx->setPoint(3, 3, true);
            mx->setPoint(3, 4, true);
            mx->setPoint(4, 3, true);
            mx->setPoint(4, 4, true);
        } else {
            bootSequenceStep++;
            modeStartTime = now;
        }
    }
    // Step 1: Small square (0-200ms)
    else if (bootSequenceStep == 1) {
        if (elapsed < 200) {
            for (int r = 3; r <= 4; r++) {
                for (int c = 3; c <= 4; c++) {
                    mx->setPoint(r, c, true);
                }
            }
        } else {
            bootSequenceStep++;
            modeStartTime = now;
        }
    }
    // Step 2: Medium square (0-200ms)
    else if (bootSequenceStep == 2) {
        if (elapsed < 200) {
            for (int r = 2; r <= 5; r++) {
                for (int c = 2; c <= 5; c++) {
                    mx->setPoint(r, c, true);
                }
            }
        } else {
            bootSequenceStep++;
            modeStartTime = now;
        }
    }
    // Step 3: Large square (0-200ms)
    else if (bootSequenceStep == 3) {
        if (elapsed < 200) {
            for (int r = 1; r <= 6; r++) {
                for (int c = 1; c <= 6; c++) {
                    mx->setPoint(r, c, true);
                }
            }
        } else {
            bootSequenceStep++;
            modeStartTime = now;
        }
    }
    // Step 4: Full border then heart (0-500ms)
    else if (bootSequenceStep == 4) {
        if (elapsed < 300) {
            // Border
            for (int i = 0; i < 8; i++) {
                mx->setPoint(0, i, true);
                mx->setPoint(7, i, true);
                mx->setPoint(i, 0, true);
                mx->setPoint(i, 7, true);
            }
        } else if (elapsed < 500) {
            // Show heart
            drawIcon(ICON_HEART);
            return;  // Don't clear, return early
        } else {
            // Boot sequence complete
            setMode(MODE_IDLE);
            return;
        }
    }
    
    mx->update();
}

// ========================================
// IDLE ANIMATIONS
// ========================================

void DisplayManager::setIdleAnimation(IdleAnimationType type) {
    currentIdleAnimation = type;
}

void DisplayManager::enableIdleAnimations(bool enable) {
    if (enable && currentMode == MODE_IDLE) {
        setMode(MODE_IDLE_ANIMATION);
    } else if (!enable && currentMode == MODE_IDLE_ANIMATION) {
        setMode(MODE_IDLE);
    }
}

void DisplayManager::updateIdleAnimation() {
    unsigned long now = millis();
    
    // Rotate through idle animations every 10 seconds
    if (now - idleAnimationStartTime > DISPLAY_IDLE_ANIMATION_DURATION) {
        currentIdleAnimation = (IdleAnimationType)((currentIdleAnimation + 1) % IDLE_COUNT);
        idleAnimationStartTime = now;
        
        // Reinitialize animation-specific state
        if (currentIdleAnimation == IDLE_MATRIX_RAIN) {
            for (int i = 0; i < 8; i++) {
                rainColumns[i].position = random(-8, 0);
            }
        } else if (currentIdleAnimation == IDLE_STARFIELD) {
            for (int i = 0; i < 8; i++) {
                stars[i].brightness = random(2);
            }
        }
    }
    
    // Update current animation
    switch (currentIdleAnimation) {
        case IDLE_BREATHING:
            animateBreathing();
            break;
        case IDLE_MATRIX_RAIN:
            animateMatrixRain();
            break;
        case IDLE_WAVE:
            animateWave();
            break;
        case IDLE_STARFIELD:
            animateStarfield();
            break;
        case IDLE_SNAKE:
            animateSnake();
            break;
        default:
            break;
    }
}

void DisplayManager::animateBreathing() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate < 50) return;
    lastAnimationUpdate = now;
    
    mx->clear();
    
    // Breathing border effect
    unsigned long phase = (now - idleAnimationStartTime) % 2000;  // 2 second cycle
    bool show = (phase < 1000);  // On for 1s, off for 1s
    
    if (show) {
        // Draw border
        for (int i = 0; i < 8; i++) {
            mx->setPoint(0, i, true);
            mx->setPoint(7, i, true);
            mx->setPoint(i, 0, true);
            mx->setPoint(i, 7, true);
        }
    }
    
    mx->update();
}

void DisplayManager::animateMatrixRain() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate < 100) return;
    lastAnimationUpdate = now;
    
    mx->clear();
    
    // Update and draw each rain column
    for (int col = 0; col < 8; col++) {
        rainColumns[col].position += rainColumns[col].speed;
        
        // Draw column
        for (int i = 0; i < rainColumns[col].length; i++) {
            int row = rainColumns[col].position - i;
            if (row >= 0 && row < 8) {
                mx->setPoint(row, col, true);
            }
        }
        
        // Reset column when it goes off bottom
        if (rainColumns[col].position > 8 + rainColumns[col].length) {
            rainColumns[col].position = random(-8, 0);
            rainColumns[col].speed = random(1, 4);
            rainColumns[col].length = random(2, 5);
        }
    }
    
    mx->update();
}

void DisplayManager::animateWave() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate < 80) return;
    lastAnimationUpdate = now;
    
    mx->clear();
    
    wavePhase += 0.3;
    if (wavePhase > 6.28) wavePhase -= 6.28;  // Keep in 0-2π range
    
    // Draw sine wave
    for (int col = 0; col < 8; col++) {
        float angle = wavePhase + (col * 0.785);  // π/4 per column
        int row = 4 + (int)(3.0 * sin(angle));
        if (row >= 0 && row < 8) {
            mx->setPoint(row, col, true);
        }
    }
    
    mx->update();
}

void DisplayManager::animateStarfield() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate < 150) return;
    lastAnimationUpdate = now;
    
    mx->clear();
    
    // Update and draw stars
    for (int i = 0; i < 8; i++) {
        // Twinkle effect
        stars[i].brightness += stars[i].twinkleDirection;
        
        if (stars[i].brightness >= 3) {
            stars[i].twinkleDirection = -1;
            stars[i].brightness = 3;
        } else if (stars[i].brightness <= 0) {
            stars[i].twinkleDirection = 1;
            stars[i].brightness = 0;
            // Occasionally move star to new position
            if (random(10) < 3) {
                stars[i].x = random(8);
                stars[i].y = random(8);
            }
        }
        
        // Draw star (simple on/off for now)
        if (stars[i].brightness > 1) {
            mx->setPoint(stars[i].y, stars[i].x, true);
        }
    }
    
    mx->update();
}

void DisplayManager::animateSnake() {
    unsigned long now = millis();
    
    if (now - lastAnimationUpdate < 200) return;
    lastAnimationUpdate = now;
    
    mx->clear();
    
    // Move snake head
    uint8_t newX = snake[0].x;
    uint8_t newY = snake[0].y;
    
    switch (snakeDirection) {
        case 0: newY = (newY == 0) ? 7 : newY - 1; break;  // up
        case 1: newX = (newX == 7) ? 0 : newX + 1; break;  // right
        case 2: newY = (newY == 7) ? 0 : newY + 1; break;  // down
        case 3: newX = (newX == 0) ? 7 : newX - 1; break;  // left
    }
    
    // Shift body
    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0].x = newX;
    snake[0].y = newY;
    
    // Random direction change
    if (random(10) < 3) {
        snakeDirection = random(4);
    }
    
    // Draw snake
    for (int i = 0; i < snakeLength; i++) {
        mx->setPoint(snake[i].y, snake[i].x, true);
    }
    
    mx->update();
}

// ========================================
// STATUS INDICATORS
// ========================================

void DisplayManager::setWiFiSignal(uint8_t level) {
    if (level > 3) level = 3;
    wifiSignalLevel = level;
}

void DisplayManager::setHAConnection(bool connected) {
    haConnected = connected;
}

void DisplayManager::showStatusBar(bool show) {
    statusBarEnabled = show;
}

// ========================================
// UTILITY
// ========================================

void DisplayManager::clear() {
    mx->clear();
    mx->update();
}

void DisplayManager::flash(uint16_t duration) {
    // Fill display
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            mx->setPoint(row, col, true);
        }
    }
    mx->update();
    
    delay(duration);
    
    clear();
}
