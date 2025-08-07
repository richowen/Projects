#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "../core/Config.h"
#include "../hal/Lcd.h"

class WiFiManagerMM {
public:
    void begin(HAL::Lcd* lcdPtr = nullptr) {
        lcd = lcdPtr;
        if (lcd) {
            lcd->clear();
            lcd->print(0, 0, "Connecting WiFi");
            lcd->print(0, 1, "Scanning...");
        }

        // Reset WiFi
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);

        // Optional: scan to lock best BSSID (keep simple initial implementation)
        // Configure static IP
        IPAddress ip(Net::IP[0], Net::IP[1], Net::IP[2], Net::IP[3]);
        IPAddress gw(Net::GATE[0], Net::GATE[1], Net::GATE[2], Net::GATE[3]);
        IPAddress sn(Net::SUBNET[0], Net::SUBNET[1], Net::SUBNET[2], Net::SUBNET[3]);
        IPAddress d1(Net::DNS1[0], Net::DNS1[1], Net::DNS1[2], Net::DNS1[3]);
        IPAddress d2(Net::DNS2[0], Net::DNS2[1], Net::DNS2[2], Net::DNS2[3]);
        WiFi.config(ip, gw, sn, d1, d2);

        WiFi.begin(Net::WIFI_SSID, Net::WIFI_PASS);

        unsigned long start = millis();
        const unsigned long timeout = 10000;
        int dot = 0;
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
            delay(250);
            if (lcd) {
                if (dot >= 16) {
                    lcd->print(0, 1, "                ");
                    dot = 0;
                }
                String s(dot + 1, '.');
                lcd->print(0, 1, s);
                dot++;
            }
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            if (lcd) {
                lcd->clear();
                lcd->print(0, 0, "WiFi Connected");
                lcd->print(0, 1, WiFi.localIP().toString());
            }
            connected = true;
        } else {
            if (lcd) {
                lcd->clear();
                lcd->print(0, 0, "WiFi Failed");
                lcd->print(0, 1, "Offline Mode");
            }
            connected = false;
        }
        lastAttempt = millis();
    }

    bool isConnected() const { return connected && WiFi.status() == WL_CONNECTED; }
    IPAddress localIP() const { return WiFi.localIP(); }

    void loop() {
        if (!connected && (millis() - lastAttempt) >= 10000) {
            begin(lcd);
        }
    }

private:
    HAL::Lcd* lcd{nullptr};
    bool connected{false};
    unsigned long lastAttempt{0};
};