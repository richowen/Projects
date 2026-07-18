# ESP32 Home Assistant Control Panel

A physical control panel using ESP32 to control Home Assistant entities via buttons, switches, potentiometers, and a MAX7219 dot matrix display.

## Features

- **Live Web UI**: Graphical web interface hosted on the ESP32 itself, showing a visual layout of the physical panel. Reassign any button/switch to a different Home Assistant entity/service live, no reflashing required.
- **WiFi Connectivity**: Connects to Home Assistant via REST API
- **Physical Controls**:

  - AC power on/off button
  - AC automation bypass toggle
  - PC shutdown button
  - Lights on/off button
  - Immersion heater toggle
  - 5 additional configurable buttons
  - Potentiometer for AC temperature control
- **Enhanced Visual Feedback**:
  - 8x8 LED matrix with custom pixel art icons for every action
  - Animated boot sequence
  - Success/error confirmations (checkmarks, X marks)
  - Progress bars for long-running actions
  - 5 different idle animations (screensavers)
  - WiFi signal strength indicators
  - Real-time temperature display
  - Quick flash feedback on button press
  - Status LED indicator
  - Serial monitor debugging

## Hardware Requirements

### Components
- **ESP32 Development Board** (esp32dev)
- **MAX7219 Dot Matrix Display** (4x 8x8 modules)
- **Potentiometer** (10kΩ recommended)
- **Buttons/Switches** (10x momentary or toggle switches)
- **Status LED** (optional, built-in LED can be used)
- **Resistors** (if needed for buttons without internal pull-ups)

### Pin Connections (Default)

#### MAX7219 Display (SPI)
- CS (Chip Select): GPIO 5
- CLK (Clock): GPIO 18
- DIN (Data): GPIO 23

#### Potentiometer
- Signal: GPIO 34 (ADC1 channel)
- VCC: 3.3V
- GND: GND

#### Buttons/Switches (with internal pull-up)
| Button | GPIO | Function |
|--------|------|----------|
| AC Power | 12 | Toggle AC unit |
| AC Bypass | 13 | Toggle automation bypass |
| PC Off | 14 | Trigger PC shutdown |
| Lights | 15 | Toggle room lights |
| Immersion | 16 | Toggle immersion heater |
| Extra 1 | 17 | Configurable |
| Extra 2 | 19 | Configurable |
| Extra 3 | 21 | Configurable |
| Extra 4 | 22 | Configurable |
| Extra 5 | 25 | Configurable |

#### Status LED
- LED: GPIO 2 (built-in LED)

**Note**: All pin assignments can be changed in `include/config.h`

## Software Setup

### 1. Install PlatformIO

Install PlatformIO IDE extension in VS Code or use PlatformIO CLI.

### 2. Clone/Download Project

```bash
git clone <your-repo-url>
cd Control-Panel
```

### 3. Configure Settings

Edit `include/config.h` and update:

#### WiFi Credentials
```cpp
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
```

#### Home Assistant Settings
```cpp
#define HA_URL "http://192.168.1.100:8123"  // Your HA IP address
#define HA_TOKEN "eyJ0eXAiOiJKV1Q..."       // Your long-lived access token
```

#### Entity IDs
Update to match your Home Assistant entity names:
```cpp
#define ENTITY_AC_UNIT "climate.your_ac"
#define ENTITY_LIGHTS "light.your_lights"
// ... etc
```

### 4. Generate Home Assistant Access Token

1. Open Home Assistant
2. Click on your profile (bottom left)
3. Scroll to "Long-Lived Access Tokens"
4. Click "Create Token"
5. Give it a name (e.g., "ESP32 Control Panel")
6. Copy the token and paste it in `config.h`

### 5. Build and Upload

```bash
pio run --target upload
```

Or use the PlatformIO upload button in VS Code.

### 6. Upload the Web UI Filesystem

The graphical web UI (HTML/CSS/JS) lives in the `data/` directory and is uploaded to the ESP32's LittleFS filesystem **separately** from the firmware:

```bash
pio run --target uploadfs
```

Run this once after the first firmware upload, and again any time you change files in `data/`. It does not require re-flashing the firmware.

### 7. Monitor Serial Output


```bash
pio device monitor
```

Or use the Serial Monitor in VS Code (115200 baud).

## Web UI - Live Configuration

Once the firmware and filesystem are uploaded and the panel connects to WiFi, a graphical web UI is available for viewing and editing the function of each physical button/switch **without recompiling or reflashing**.

### Accessing the Web UI

- Via mDNS (if supported by your network/OS): `http://controlpanel.local`
- Via IP address: check the serial monitor output at boot for the assigned IP (also printed after `[INIT] Web Server... OK`)

### What You Can Do

- View a graphical layout of the panel mirroring the physical device (toggle switches, push buttons, display, and potentiometer).
- Click any button/switch tile to open an edit form and change its:
  - **Entity ID** (e.g. `light.living_room`)
  - **Service/domain** (e.g. `toggle`, `switch`, `trigger`)
  - **Type** (Momentary Button or Toggle Switch)
- Changes are saved to persistent storage (NVS) on the ESP32 and take effect immediately - no reboot needed.

### Notes

- WiFi credentials and the Home Assistant URL/token remain fixed in `include/secrets.h` and are **not** editable via the web UI.
- The web UI has **no authentication** - it is intended for use on a trusted local network only.
- If you edit files in `data/`, re-run `pio run --target uploadfs` to push the changes to the device.

## Configuration


### Adjusting Pin Assignments

Edit `include/config.h`:

```cpp
#define BTN_AC_POWER_PIN 12  // Change to your desired GPIO
```

### Temperature Range

Adjust potentiometer temperature range:

```cpp
#define TEMP_MIN 16  // Minimum temperature (°C)
#define TEMP_MAX 30  // Maximum temperature (°C)
```

### Display Brightness

```cpp
#define DISPLAY_INTENSITY 5  // 0-15 (0=dim, 15=bright)
```

### Timing Settings

```cpp
#define DEBOUNCE_DELAY 50          // Button debounce (ms)
#define POT_SEND_INTERVAL 1000     // Potentiometer update rate (ms)
#define TEMP_UPDATE_INTERVAL 5000  // Display refresh rate (ms)
```

## Home Assistant Entity Configuration

### Required Entity Types

The control panel expects these entity types:

- **Climate entities**: `climate.*` (for AC control)
- **Light entities**: `light.*`
- **Switch entities**: `switch.*`
- **Button entities**: `button.*`
- **Input Boolean**: `input_boolean.*`

### Example Home Assistant Configuration

```yaml
# configuration.yaml

# AC automation bypass toggle
input_boolean:
  ac_automation_bypass:
    name: AC Automation Bypass
    icon: mdi:air-conditioner

# PC shutdown button
button:
  - platform: template
    buttons:
      pc_shutdown:
        friendly_name: "PC Shutdown"
        press:
          - service: hassio.addon_stdin
            data:
              addon: core_ssh
              input: "shutdown now"
```

## Troubleshooting

### WiFi Won't Connect
- Check SSID and password in `config.h`
- Ensure ESP32 is within WiFi range
- Check serial monitor for error messages

### Display Not Working
- Verify MAX7219 wiring (CS, CLK, DIN)
- Check `MAX_DEVICES` matches your display chain length
- Try adjusting `DISPLAY_INTENSITY`

### Buttons Not Responding
- Verify GPIO pins are correctly wired
- Check serial monitor for button press messages
- Ensure buttons are wired with pull-up configuration (or use internal pull-up)

### Home Assistant Commands Not Working
- Verify HA_URL is correct (include `http://`)
- Check access token is valid
- Ensure entity IDs match exactly (case-sensitive)
- Check Home Assistant logs for API errors

### Temperature Not Updating
- Verify entity ID in `ENTITY_AC_UNIT`
- Check if entity provides temperature as state
- May need to read from attributes instead (modify code)

## Serial Monitor Output

Expected startup sequence:
```
=================================
Home Control Panel Starting...
=================================

Initializing MAX7219 Display...
Initializing Buttons...
Connecting to WiFi: YourNetwork
....
WiFi Connected!
IP Address: 192.168.1.50

Setup Complete!
```

## Code Structure

```
Control Panel/
├── include/
│   ├── config.h          # User configuration
│   └── README
├── src/
│   └── main.cpp          # Main program
├── platformio.ini        # PlatformIO config
└── README.md            # This file
```

## API Reference

### Home Assistant REST API Endpoints

The panel uses these endpoints:

**Send Command:**
```
POST /api/services/{domain}/{service}
Headers:
  Authorization: Bearer {token}
  Content-Type: application/json
Body:
  {
    "entity_id": "domain.entity_name",
    "additional": "parameters"
  }
```

**Get State:**
```
GET /api/states/{entity_id}
Headers:
  Authorization: Bearer {token}
```

## Customization

### Adding New Buttons

1. Define pin in `config.h`:
```cpp
#define BTN_CUSTOM_PIN 26
```

2. Define entity in `config.h`:
```cpp
#define ENTITY_CUSTOM "switch.my_device"
```

3. Add to button array in `main.cpp`:
```cpp
{BTN_CUSTOM_PIN, HIGH, HIGH, 0, ENTITY_CUSTOM, "toggle"}
```

### Modifying Display Content

Edit the `updateTemperatureDisplay()` function in `main.cpp` to change what's displayed.

## License

MIT License - Feel free to modify and use for your own projects.

## Support

For issues or questions:
1. Check the troubleshooting section
2. Review serial monitor output
3. Verify Home Assistant configuration
4. Check PlatformIO build output

## Version History

- **v2.0.0** - Enhanced Display UX
  - Complete 8x8 LED display UX overhaul
  - 27 custom pixel art icons
  - Animated boot sequence
  - 5 idle animation patterns (matrix rain, waves, breathing, starfield, snake)
  - Success/error visual feedback
  - Progress bars (vertical, horizontal)
  - WiFi status indicators
  - Configurable animations and timeouts
  - Memory optimized (icons in PROGMEM)
  - See [DISPLAY_UX_ARCHITECTURE.md](DISPLAY_UX_ARCHITECTURE.md) for details

- **v1.0.0** - Initial release
  - WiFi connectivity
  - 10 button inputs
  - Potentiometer control
  - MAX7219 display
  - Home Assistant REST API integration