# ESP32 Air Quality Monitor - Rainmeter Widget

Desktop widget that displays real-time air quality data from your ESP32 sensor via the Home Assistant REST API.

## 📋 Prerequisites

1. **Rainmeter installed** - Download from https://www.rainmeter.net/
2. **ESP32 running** and publishing to Home Assistant
3. **Home Assistant accessible** at http://192.168.1.3:8123
4. **MQTT integration configured** in Home Assistant
5. **All 8 sensors auto-discovered** in Home Assistant

## 🚀 Installation

### Step 1: Copy Files to Rainmeter

Copy the entire `Rainmeter` folder to your Rainmeter Skins directory:

**Default location:**
```
C:\Users\YourUsername\Documents\Rainmeter\Skins\AirQuality\
```

Your folder structure should look like:
```
Documents/Rainmeter/Skins/AirQuality/
├── AirQuality.ini
├── Variables.inc
└── README.md (this file)
```

### Step 2: Get Home Assistant Long-Lived Access Token

1. Open Home Assistant web interface
2. Click on your **Profile** (bottom left)
3. Scroll down to **Security** section
4. Under **Long-Lived Access Tokens**, click **CREATE TOKEN**
5. Give it a name like "Rainmeter Air Quality"
6. **Copy the token** (you won't be able to see it again!)

### Step 3: Configure Variables

Edit `Variables.inc` and update these settings:

```ini
; Your Home Assistant IP and port
HAHost=192.168.1.3
HAPort=8123

; Paste your token here (replace YOUR_TOKEN_HERE)
HAToken=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiI...
```

**IMPORTANT:** The token is very long (hundreds of characters). Make sure you copy the entire token!

### Step 4: Verify Entity IDs

The widget expects these entity IDs in Home Assistant:
- `sensor.air_quality_pm1`
- `sensor.air_quality_pm25`
- `sensor.air_quality_pm10`
- `sensor.air_quality_tvoc`
- `sensor.air_quality_eco2`
- `sensor.air_quality_aqi`
- `sensor.air_quality_co`
- `sensor.air_quality_o2`

Check these exist in Home Assistant:
1. Settings → Devices & Services → MQTT → ESP32 Air Quality Monitor
2. Click on the device
3. Verify all 8 sensors are listed

### Step 5: Load the Skin

1. **Right-click Rainmeter tray icon** (bottom right of screen)
2. **Manage**
3. In the **Skins** list, find **AirQuality**
4. Click **AirQuality.ini**
5. Click **Load** button (bottom right)

The widget should appear on your desktop!

## 🎨 Widget Display

The widget shows:

```
┌─────────────────────────────┐
│ 🌫️ Air Quality Monitor      │
├─────────────────────────────┤
│ Particulate Matter          │
│  PM1.0:  12 μg/m³           │
│  PM2.5:  28 μg/m³  ████     │
│  PM10:   42 μg/m³           │
├─────────────────────────────┤
│ Gas Concentrations          │
│  TVOC:   245 ppb            │
│  eCO2:   650 ppm            │
│  CO:     2.10 ppm           │
│  O2:     20.80 %            │
├─────────────────────────────┤
│ Air Quality Index           │
│  3  Moderate                │
├─────────────────────────────┤
│ Last updated: 2 seconds ago │
│ ● Connected                 │
└─────────────────────────────┘
```

## 🎨 Color Coding

### PM2.5 Levels
- **Green** (0-12 μg/m³) - Good
- **Yellow** (12-35 μg/m³) - Moderate  
- **Red** (35+ μg/m³) - Unhealthy

### AQI Scale
- **1** - Excellent (Green)
- **2** - Good (Light Green)
- **3** - Moderate (Yellow)
- **4** - Poor (Orange)
- **5** - Very Poor (Red)

## ⚙️ Customization

### Change Update Frequency

Edit `Variables.inc`:
```ini
; Update every 15 seconds instead of 30
UpdateRate=15
```

### Change Colors

Edit `Variables.inc`:
```ini
ColorBackground=25,25,30,230    ; Dark background
ColorText=255,255,255,255       ; White text
ColorExcellent=76,175,80,255    ; Green for good air
ColorModerate=255,193,7,255     ; Yellow for moderate
ColorPoor=244,67,54,255         ; Red for poor air
```

Format: R,G,B,Alpha (0-255 each)

### Adjust Size

Edit `Variables.inc`:
```ini
WidgetWidth=320    ; Width in pixels
WidgetHeight=420   ; Height in pixels
```

### Change Position

Right-click widget → Settings → Position → Choose screen position

Or drag the widget where you want it, then:
Right-click → Settings → Enable "Save position"

## 🐛 Troubleshooting

### Widget Shows "0" or "N/A" for All Values

**Problem:** Can't connect to Home Assistant

**Solutions:**
1. Verify Home Assistant IP is correct in `Variables.inc`
2. Check token is valid and complete (no spaces/line breaks)
3. Test in browser: `http://192.168.1.3:8123/api/states`
4. Check Home Assistant is running

### "Connection Lost" or Red Text

**Problem:** WebParser can't fetch data

**Solutions:**
1. Check WiFi connection on your PC
2. Verify Home Assistant is accessible
3. Try creating a new access token
4. Check entity IDs match in Home Assistant

### Wrong Entity Names

**Problem:** Entity IDs don't match

Edit each `[MeasureXXX]` URL in `AirQuality.ini`:
```ini
; Change this:
URL=http://#HAHost#:#HAPort#/api/states/sensor.air_quality_pm25

; To match your actual entity ID:
URL=http://#HAHost#:#HAPort#/api/states/sensor.your_actual_entity_id
```

### Values Don't Update

**Problem:** Update rate too fast or HA not responding

1. Increase `UpdateRate` in `Variables.inc` to 60 seconds
2. Check Home Assistant logs for API errors
3. Verify ESP32 is publishing data (check HA states)

## 🔍 Testing the Widget

### Test 1: Manual API Call

Open browser and go to:
```
http://192.168.1.3:8123/api/states/sensor.air_quality_pm25
```

You should see JSON like:
```json
{
  "entity_id": "sensor.air_quality_pm25",
  "state": "28",
  "attributes": {
    "unit_of_measurement": "μg/m³",
    "device_class": "pm25"
  }
}
```

Add your token as header to test authentication:
- Use Postman or curl
- Header: `Authorization: Bearer YOUR_TOKEN`

### Test 2: Check Rainmeter Log

1. Right-click Rainmeter tray icon
2. **About** → **Log** tab
3. Look for WebParser errors
4. Check if API calls are being made

### Test 3: Refresh Skin

If you make changes:
1. Right-click widget
2. **Refresh skin**

Or reload all:
1. Right-click Rainmeter tray icon
2. **Refresh all**

## 📊 Understanding the Data

### Particulate Matter (PM)
- **PM1.0**: Particles ≤ 1.0 micrometers
- **PM2.5**: Particles ≤ 2.5 micrometers (most harmful to lungs)
- **PM10**: Particles ≤ 10 micrometers

**Typical Values:**
- Good: < 12 μg/m³
- Moderate: 12-35 μg/m³
- Unhealthy: > 35 μg/m³

### Gases
- **TVOC**: Total Volatile Organic Compounds (paint, cleaning products, etc.)
- **eCO2**: Equivalent CO2 (based on VOCs, not actual CO2)
- **CO**: Carbon Monoxide (dangerous gas)
- **O2**: Oxygen level (normal is ~20.9%)

### Air Quality Index (AQI)
- **1**: Excellent - No health concerns
- **2**: Good - Safe for everyone
- **3**: Moderate - Sensitive people should limit outdoor activity
- **4**: Poor - Everyone should limit outdoor activity
- **5**: Very Poor - Everyone should avoid outdoor activity

## 🔧 Advanced Features

### Make Widget Click-Through

Right-click widget → Settings → Enable "Click through"

### Keep Widget Always on Top

Right-click widget → Settings → Position → "Stay topmost"

### Load on Windows Startup

Right-click Rainmeter tray icon → Manage → Settings → Enable "Run Rainmeter at startup"

### Add Fade Effects

Add to `[Rainmeter]` section in `AirQuality.ini`:
```ini
[Rainmeter]
FadeIn=250
FadeOut=250
```

## 📝 Next Steps

Once the widget is working:
1. Position it where you want on your desktop
2. Adjust colors and size to match your desktop theme
3. Set up alerts for poor air quality (optional)
4. Add multiple widgets if you have multiple ESP32 sensors

## 💡 Tips

- The widget updates every 30 seconds to match ESP32 publish rate
- Color changes automatically based on air quality thresholds
- The AQI value is the overall air quality summary
- PM2.5 is the most important metric for health
- Normal O2 is around 20.9% - deviation indicates sensor issues

## 🆘 Getting Help

If you have issues:
1. Check Rainmeter log for errors
2. Verify all entity IDs exist in Home Assistant
3. Test API manually in browser
4. Check token hasn't expired
5. Ensure ESP32 is publishing data

---

**Status:** Ready to install
**Last Updated:** 2025-12-13