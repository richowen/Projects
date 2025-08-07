#pragma once
#include <Arduino.h>
#include <DFRobot_RGBLCD1602.h>
#include "../core/Config.h"

namespace HAL {

class Lcd {
public:
    void begin() {
        lcd = new DFRobot_RGBLCD1602(LcdCfg::I2C_ADDR, LcdCfg::COLS, LcdCfg::ROWS);
        lcd->init();
        setRGB(LcdCfg::BOOT_R, LcdCfg::BOOT_G, LcdCfg::BOOT_B);
        clear();
        print(0, 0, "Milk Mixer V5");
        print(0, 1, "Booting...");
    }

    void setIdleColor()   { setRGB(LcdCfg::IDLE_R, LcdCfg::IDLE_G, LcdCfg::IDLE_B); }
    void setMixColor()    { setRGB(LcdCfg::MIX_R,  LcdCfg::MIX_G,  LcdCfg::MIX_B); }
    void setErrorColor()  { setRGB(LcdCfg::ERR_R,  LcdCfg::ERR_G,  LcdCfg::ERR_B); }
    void setWashColor()   { setRGB(LcdCfg::WASH_R, LcdCfg::WASH_G, LcdCfg::WASH_B); }
    void setOtaColor()    { setRGB(LcdCfg::OTA_R,  LcdCfg::OTA_G,  LcdCfg::OTA_B); }

    void clear() {
        lcd->clear();
        lastLine1 = "";
        lastLine2 = "";
    }

    // Diff-only updates to reduce I2C chatter and flicker
    void print(uint8_t col, uint8_t row, const String& text) {
        if (row == 0) {
            if (text == lastLine1) return;
            lastLine1 = text;
        } else {
            if (text == lastLine2) return;
            lastLine2 = text;
        }
        lcd->setCursor(col, row);
        lcd->print(pad(text));
    }

    void showIdle() {
        setIdleColor();
        print(0, 0, "Status: IDLE");
        print(0, 1, "Ready to Start");
    }

    void showMixing() {
        setMixColor();
        print(0, 0, "Status: MIXING");
        print(0, 1, "Running...");
    }

    void showPostMix(uint16_t remainingSec) {
        setMixColor();
        print(0, 0, String("Post-Mix: ") + remainingSec + "s");
        print(0, 1, "Finishing...");
    }

    void showError(const char* msg) {
        setErrorColor();
        print(0, 0, "ERROR");
        print(0, 1, msg);
    }

    void showWash(bool waterOn) {
        setWashColor();
        print(0, 0, "WASH MODE");
        print(0, 1, waterOn ? "Water: ON" : "Water: OFF");
    }

private:
    DFRobot_RGBLCD1602* lcd{nullptr};
    String lastLine1{};
    String lastLine2{};

    void setRGB(uint8_t r, uint8_t g, uint8_t b) { lcd->setRGB(r, g, b); }

    // Pad and trim to LCD width
    static String pad(const String& s) {
        String out = s;
        if (out.length() > LcdCfg::COLS) {
            out.remove(LcdCfg::COLS);
        }
        while (out.length() < LcdCfg::COLS) {
            out += ' ';
        }
        return out;
    }
};

} // namespace HAL