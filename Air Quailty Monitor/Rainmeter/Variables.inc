[Variables]
; =====================================
; Home Assistant Configuration
; =====================================
; IMPORTANT: You MUST update these values!
; 1. Get Long-Lived Access Token from Home Assistant:
;    Profile → Security → Long-Lived Access Tokens → Create Token
; 2. Replace YOUR_TOKEN_HERE with your actual token

HAHost=192.168.1.3
HAPort=8123
HAToken=YOUR_TOKEN_HERE

; =====================================
; Update Settings
; =====================================
; How often to fetch data (in seconds)
; Match ESP32 update interval (30 seconds)
UpdateRate=30

; =====================================
; Color Scheme
; =====================================
ColorBackground=25,25,30,230
ColorText=255,255,255,255
ColorTextDim=180,180,180,255
ColorExcellent=76,175,80,255
ColorGood=139,195,74,255
ColorModerate=255,193,7,255
ColorPoor=255,152,0,255
ColorVeryPoor=244,67,54,255

; =====================================
; Air Quality Thresholds (PM2.5 based)
; =====================================
; PM2.5 levels (μg/m³):
; 0-12: Good
; 12-35: Moderate
; 35-55: Unhealthy for Sensitive Groups
; 55+: Unhealthy
ThresholdGood=12
ThresholdModerate=35
ThresholdPoor=55

; =====================================
; Layout Settings
; =====================================
WidgetWidth=320
WidgetHeight=420
FontFace=Segoe UI
FontSize=10
TitleSize=13
ValueSize=11