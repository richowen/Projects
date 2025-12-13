# ESP32 Air Quality Monitor

Multi-sensor air quality monitoring system that publishes data to Home Assistant via MQTT with auto-discovery, and displays real-time data on a Rainmeter desktop widget.

## 📋 Hardware Components

- **ESP32 DEVKIT 1** - Main microcontroller
- **PM2.5 Sensor** (DFRobot SEN0460) @ I2C 0x19 - Particulate matter detection
- **ENS160 Multi-Gas Sensor** @ I2C 0x53 - TVOC, eCO2, and AQI
- **CO Gas Sensor** @ I2C 0x74 - Carbon monoxide detection
- **O2 Gas Sensor** @ I2C 0x76 - Oxygen percentage

## 🚀 Quick Start

### 1. Flash the ESP32

```bash
# Build and upload the firmware
pio run --target upload

# Monitor serial output
pio device monitor
```

### 2. Verify Sensors

After uploading, check the Serial Monitor (115200 baud) for:
```
=================================
ESP32 Air Quality Monitor
=================================

I2C initialized
Connecting to WiFi...
WiFi connected!
IP address: 192.168.1.25

Initializing sensors...
-------------------------
PM2.5 Sensor (0x19): OK
ENS160 Sensor (0x53): OK
CO Sensor (0x74): OK
O2 Sensor (0x76): OK
-------------------------
Sensors initialized: 4 of 4
```

### 3. Check Home Assistant

1. Navigate to **Settings → Devices & Services → MQTT**
2. Look for **ESP32 Air Quality Monitor**
3. Verify all 8 sensor entities are discovered:
   - Air Quality PM1.0
   - Air Quality PM2.5
   - Air Quality PM10
   - Air Quality TVOC
   - Air Quality eCO2
   - Air Quality AQI
   - Air Quality CO
   - Air Quality O2

### 4. View Live Data

Check the Serial Monitor every 30 seconds for readings:
```
--- Sensor Reading ---
PM1.0: 12 | PM2.5: 28 | PM10: 42 μg/m³
TVOC: 245 ppb | eCO2: 650 ppm | AQI: 3
CO: 2.10 ppm
O2: 20.80%
----------------------
```

### 5. Install Rainmeter Desktop Widget

Copy the [`Rainmeter/`](Rainmeter/) folder to your Rainmeter skins directory and follow the setup in [`Rainmeter/INSTALL.md`](Rainmeter/INSTALL.md).

**Quick steps:**
1. Install Rainmeter from https://www.rainmeter.net/
2. Copy `Rainmeter` folder to `Documents\Rainmeter\Skins\AirQuality\`
3. Get Home Assistant Long-Lived Access Token
4. Edit [`Rainmeter/Variables.inc`](Rainmeter/Variables.inc) with your token
5. Load skin in Rainmeter Manager

See [`Rainmeter/README.md`](Rainmeter/README.md) for detailed instructions.

## 📊 Sensor Data

| Sensor | Measurements | Unit | Home Assistant Entity |
|--------|--------------|------|----------------------|
| PM2.5  | PM1.0, PM2.5, PM10 | μg/m³ | sensor.air_quality_pm1, pm25, pm10 |
| ENS160 | TVOC, eCO2, AQI | ppb, ppm, 1-5 | sensor.air_quality_tvoc, eco2, aqi |
| CO     | Carbon Monoxide | ppm | sensor.air_quality_co |
| O2     | Oxygen | % | sensor.air_quality_o2 |

## ⚙️ Configuration

All configuration is in [`include/config.h`](include/config.h):

```cpp
// WiFi Settings
#define WIFI_SSID "WiFi"
#define WIFI_PASSWORD "Gliders!"
#define STATIC_IP IPAddress(192, 168, 1, 25)

// MQTT Settings
#define MQTT_SERVER "192.168.1.3"
#define MQTT_PORT 1883
#define MQTT_USER "richowen"
#define MQTT_PASSWORD "p"

// Update interval (30 seconds)
#define UPDATE_INTERVAL 30000
```

## 🏗️ Project Structure

```
├── platformio.ini          # PlatformIO configuration with all dependencies
├── include/
│   └── config.h           # WiFi and MQTT configuration
├── src/
│   ├── main.cpp           # Main firmware with all sensor logic
│   └── info               # Project requirements reference
├── ARCHITECTURE.md         # Detailed system architecture
├── RAINMETER_PLAN.md      # Rainmeter widget implementation guide
├── IMPLEMENTATION_GUIDE.md # Step-by-step implementation guide
└── README.md              # This file
```

## ✨ Features

✅ **WiFi Connection**
- Static IP configuration (192.168.1.25)
- Automatic reconnection on connection loss
- Signal strength monitoring

✅ **MQTT Integration**
- Home Assistant auto-discovery
- Retained messages for last known values
- Automatic reconnection
- 1024-byte buffer for discovery messages

✅ **Sensor Management**
- Individual sensor status tracking
- Graceful degradation if sensors fail
- Error handling and reporting
- Test readings during initialization

✅ **Data Publishing**
- 30-second update intervals
- Non-blocking loop design
- Serial debug logging
- 8 separate sensor entities

✅ **Home Assistant Integration**
- Auto-discovery on boot
- Proper device classes and units
- Device grouping
- Icon assignments

## 🔧 Troubleshooting

### WiFi Won't Connect
- Verify SSID and password in `config.h`
- Check router allows static IP 192.168.1.25
- Monitor signal strength in Serial output

### Sensor Not Detected
- Check I2C wiring (SDA, SCL, VCC, GND)
- Verify sensor power (3.3V or 5V as required)
- Run I2C scanner to confirm addresses
- Check Serial Monitor for initialization status

### MQTT Not Connecting
- Verify broker IP (192.168.1.3) is accessible
- Check credentials in `config.h`
- Check firewall on Home Assistant host
- Monitor MQTT broker logs

### No Auto-Discovery in Home Assistant
- Verify MQTT integration is enabled in HA
- Check auto-discovery prefix is "homeassistant"
- Look for discovery messages in MQTT broker
- Re-publish by restarting ESP32

## 📈 Next Steps

### Testing Phase
1. ✅ Flash firmware to ESP32
2. ⏳ Test WiFi reconnection (router reboot)
3. ⏳ Test MQTT reconnection (broker restart)
4. ⏳ Verify all sensors in Home Assistant
5. ⏳ Monitor data for 24 hours for stability

### Rainmeter Widget Phase
1. Install Rainmeter on desktop
2. Create Long-Lived Access Token in Home Assistant
3. Follow [`RAINMETER_PLAN.md`](RAINMETER_PLAN.md) for implementation
4. Configure widget with HA token and IP

### Optional Enhancements
- Add OTA update capability
- Create web interface on ESP32
- Add sensor calibration routines
- Implement data averaging
- Add alert thresholds

## 📚 Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System design, data flow, MQTT topics
- **[RAINMETER_PLAN.md](RAINMETER_PLAN.md)** - Desktop widget implementation
- **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** - Detailed step-by-step guide

## 🔗 Resources

- [PM2.5 Sensor Wiki](https://wiki.dfrobot.com/Gravity_PM2.5_Air_Quality_Sensor_SKU_SEN0460)
- [ENS160 Sensor Wiki](https://wiki.dfrobot.com/SKU_SEN0514_Gravity_ENS160_Air_Quality_Sensor)
- [Gas Sensor Wiki](https://wiki.dfrobot.com/SKU_SEN0465toSEN0476_Gravity_Gas_Sensor_Calibrated_I2C_UART)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)

## 📝 License

This project is for personal use. Sensor libraries are subject to their respective licenses.

## 🛠️ Built With

- **PlatformIO** - Development environment
- **ESP32 Arduino Core** - Framework
- **PubSubClient** - MQTT client library
- **ArduinoJson** - JSON handling for auto-discovery
- **DFRobot Libraries** - Sensor drivers

---

**Status**: ✅ Firmware implemented and ready for testing
**Last Updated**: 2025-12-13