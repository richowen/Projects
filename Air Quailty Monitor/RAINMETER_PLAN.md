# Rainmeter Air Quality Widget - Implementation Plan

## Overview

A Rainmeter desktop widget that displays real-time air quality data from your ESP32 sensor via the Home Assistant REST API.

## Widget Layout Design

```
┌─────────────────────────────────────┐
│   🌫️ Air Quality Monitor           │
├─────────────────────────────────────┤
│                                     │
│  Particulate Matter                 │
│  PM1.0:  12 μg/m³  [████████░░]    │
│  PM2.5:  28 μg/m³  [████████░░]    │
│  PM10:   42 μg/m³  [████████░░]    │
│                                     │
│  Gases                              │
│  TVOC:   245 ppb   [████████░░]    │
│  eCO2:   650 ppm   [████████░░]    │
│  CO:     2.1 ppm   [████████░░]    │
│  O2:     20.8%     [██████████]    │
│                                     │
│  Air Quality Index: 3 (Moderate)    │
│                                     │
│  Updated: 2 seconds ago             │
└─────────────────────────────────────┘
```

## Home Assistant REST API Integration

### API Endpoint
```
http://192.168.1.3:8123/api/states/sensor.{entity_id}
```

### Authentication
- Long-Lived Access Token required
- Set in Rainmeter config as variable
- Header: `Authorization: Bearer YOUR_TOKEN`

### Entity IDs (Auto-discovered)
```
sensor.air_quality_pm1
sensor.air_quality_pm25
sensor.air_quality_pm10
sensor.air_quality_tvoc
sensor.air_quality_eco2
sensor.air_quality_aqi
sensor.air_quality_co
sensor.air_quality_o2
```

### API Response Example
```json
{
  "entity_id": "sensor.air_quality_pm25",
  "state": "28",
  "attributes": {
    "unit_of_measurement": "μg/m³",
    "device_class": "pm25",
    "friendly_name": "Air Quality PM2.5"
  },
  "last_changed": "2025-12-13T18:30:00.000000+00:00",
  "last_updated": "2025-12-13T18:30:00.000000+00:00"
}
```

## Rainmeter Skin Structure

```
Skins/
└── AirQuality/
    ├── AirQuality.ini           # Main skin file
    ├── Variables.inc            # Configuration variables
    ├── @Resources/
    │   ├── Images/
    │   │   ├── background.png
    │   │   ├── icon_pm.png
    │   │   ├── icon_gas.png
    │   │   └── icon_aqi.png
    │   └── Fonts/
    │       └── Roboto.ttf
    └── README.md                # Setup instructions
```

## Variables.inc Configuration

```ini
[Variables]
; Home Assistant Configuration
HAHost=192.168.1.3
HAPort=8123
HAToken=YOUR_LONG_LIVED_ACCESS_TOKEN_HERE

; Update Interval (in seconds)
UpdateRate=30

; Color Scheme
ColorBackground=20,20,25,220
ColorText=255,255,255,255
ColorGood=76,175,80,255
ColorModerate=255,193,7,255
ColorPoor=244,67,54,255

; Air Quality Thresholds (PM2.5 based)
ThresholdGood=12
ThresholdModerate=35
ThresholdPoor=55
```

## Main Skin File Structure (AirQuality.ini)

### Metadata Section
```ini
[Rainmeter]
Update=1000
AccurateText=1
DynamicWindowSize=1

[Metadata]
Name=ESP32 Air Quality Monitor
Author=Your Name
Version=1.0
License=Creative Commons Attribution-Non-Commercial-Share Alike 3.0
Information=Displays air quality data from ESP32 via Home Assistant
```

### WebParser Measures (API Calls)

One measure per sensor reading:

```ini
[MeasurePM25]
Measure=Plugin
Plugin=WebParser
URL=http://[&HAHost]:[&HAPort]/api/states/sensor.air_quality_pm25
Header=Authorization: Bearer [&HAToken]
RegExp=(?siU)"state": "(.+?)"
UpdateRate=#UpdateRate#
StringIndex=1

[MeasurePM25Value]
Measure=Plugin
Plugin=WebParser
URL=[MeasurePM25]
StringIndex=1
```

### Bar Meters

Visual bars showing levels relative to thresholds:

```ini
[MeterPM25Bar]
Meter=Bar
MeasureName=MeasurePM25Value
X=150
Y=50
W=100
H=10
BarColor=#ColorGood#
BarOrientation=Horizontal
DynamicVariables=1
```

### Dynamic Color Based on Thresholds

```ini
[MeterPM25Bar]
BarColor=(#MeasurePM25Value# <= #ThresholdGood# ? "#ColorGood#" : (#MeasurePM25Value# <= #ThresholdModerate# ? "#ColorModerate#" : "#ColorPoor#"))
```

### Text Displays

```ini
[MeterPM25Label]
Meter=String
X=10
Y=50
FontColor=#ColorText#
FontSize=10
Text=PM2.5:

[MeterPM25Value]
Meter=String
MeasureName=MeasurePM25Value
X=150
Y=50
FontColor=#ColorText#
FontSize=10
Text=%1 μg/m³
DynamicVariables=1
```

## Air Quality Index Color Coding

```ini
[MeterAQI]
Meter=String
MeasureName=MeasureAQI
X=10
Y=200
FontSize=14
FontWeight=700
Text=AQI: %1
DynamicVariables=1
FontColor=(#MeasureAQI# = 1 ? "76,175,80" : (#MeasureAQI# = 2 ? "139,195,74" : (#MeasureAQI# = 3 ? "255,193,7" : (#MeasureAQI# = 4 ? "255,87,34" : "244,67,54"))))
```

AQI Scale:
- 1 = Excellent (Green)
- 2 = Good (Light Green)
- 3 = Moderate (Yellow)
- 4 = Poor (Orange)
- 5 = Very Poor (Red)

## Update Timestamp

```ini
[MeasureLastUpdate]
Measure=Plugin
Plugin=WebParser
URL=[MeasurePM25]
RegExp=(?siU)"last_updated": "(.+?)"
StringIndex=1

[MeterLastUpdate]
Meter=String
MeasureName=MeasureLastUpdate
X=10
Y=240
FontSize=8
FontColor=200,200,200
Text=Updated: %1
DynamicVariables=1
```

## Error Handling

### Connection Error Display
```ini
[MeasurePM25Value]
Measure=Plugin
Plugin=WebParser
URL=[MeasurePM25]
StringIndex=1
Substitute="":"N/A"
IfCondition=(MeasurePM25Value = 0)
IfTrueAction=[!SetOption MeterPM25Value FontColor "244,67,54"]
IfFalseAction=[!SetOption MeterPM25Value FontColor "#ColorText#"]
```

### Offline Indicator
```ini
[MeterStatus]
Meter=String
X=10
Y=220
FontSize=9
Text=(Connection Lost)
FontColor=244,67,54
Hidden=1
DynamicVariables=1
```

## Implementation Steps

### 1. Create Home Assistant Long-Lived Access Token
1. Log into Home Assistant
2. Go to Profile → Security
3. Create Long-Lived Access Token
4. Copy token to Variables.inc

### 2. Create Base Skin Files
1. Create folder structure
2. Set up Variables.inc with your HA details
3. Create AirQuality.ini skeleton

### 3. Implement WebParser Measures
1. Add measures for all 8 sensors
2. Test API connectivity
3. Validate JSON parsing

### 4. Design Layout
1. Background and positioning
2. Add text labels
3. Add value displays
4. Add bar meters

### 5. Add Visual Enhancements
1. Color coding based on thresholds
2. Icons for different sensor types
3. Background with transparency
4. Smooth value transitions

### 6. Testing
1. Verify all sensor values display correctly
2. Test update intervals
3. Test error handling (disconnect HA)
4. Optimize performance

## Advanced Features (Optional)

### Historical Graphing
- Use Line meter to show trends
- Store last 60 readings in variables
- Display 24-hour history graph

### Alerts
- Desktop notifications for poor air quality
- Sound alerts for dangerous levels
- Email/SMS integration

### Multiple Locations
- Support multiple ESP32 sensors
- Switch between locations
- Compare air quality across rooms

### Mobile Companion
- Export data to mobile dashboard
- Web-based view using same data
- Push notifications to phone

## Performance Considerations

- Update Rate: 30 seconds (matches ESP32 publish rate)
- WebParser cache: Enabled
- String substitution for faster parsing
- Conditional updates to reduce CPU usage
- Background transparency: Optimized

## Troubleshooting Guide

### Common Issues

1. **No Data Displayed**
   - Check HA access token
   - Verify network connectivity
   - Check entity IDs match auto-discovered names

2. **Incorrect Values**
   - Verify RegExp patterns
   - Check JSON response format
   - Validate unit conversions

3. **High CPU Usage**
   - Increase UpdateRate
   - Disable unnecessary meters
   - Optimize RegExp patterns

4. **Visual Glitches**
   - Check DynamicVariables=1
   - Verify image paths
   - Update Rainmeter to latest version

## Resources

- Rainmeter WebParser: https://docs.rainmeter.net/manual/plugins/webparser/
- Home Assistant API: https://developers.home-assistant.io/docs/api/rest/
- Color Picker: For customizing theme colors
- JSON Validator: For testing API responses