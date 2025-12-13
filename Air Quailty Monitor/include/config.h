#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "IoT"
#define WIFI_PASSWORD "Gliders1!"

// Static IP Configuration
#define STATIC_IP IPAddress(192, 168, 1, 23)
#define GATEWAY IPAddress(192, 168, 1, 1)
#define SUBNET IPAddress(255, 255, 255, 0)
#define DNS IPAddress(192, 168, 1, 1)

// MQTT Configuration
#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_USER "richowen"
#define MQTT_PASSWORD "p"
#define MQTT_CLIENT_ID "esp32_air_quality_sensor"

// MQTT Topics Base
#define MQTT_BASE_TOPIC "homeassistant/sensor"
#define DEVICE_NAME "esp32_air_quality"

// Sensor I2C Addresses
#define PM25_I2C_ADDR 0x19
#define ENS160_I2C_ADDR 0x53
#define CO_I2C_ADDR 0x74
#define O2_I2C_ADDR 0x76

// Update Interval (milliseconds)
#define UPDATE_INTERVAL 30000  // 30 seconds

// Serial Baud Rate
#define SERIAL_BAUD 115200

#endif