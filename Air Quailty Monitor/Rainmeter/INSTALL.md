# Quick Installation Guide

## ⚡ 5-Minute Setup

### 1️⃣ Install Rainmeter
Download and install from: https://www.rainmeter.net/

### 2️⃣ Copy Widget Files

Copy the `Rainmeter` folder to:
```
C:\Users\<YourUsername>\Documents\Rainmeter\Skins\AirQuality\
```

**Or use this path directly:**
```
%USERPROFILE%\Documents\Rainmeter\Skins\AirQuality\
```

### 3️⃣ Get Home Assistant Token

1. Open Home Assistant: http://192.168.1.3:8123
2. Profile (bottom left) → Security
3. Long-Lived Access Tokens → **Create Token**
4. Name: "Rainmeter"
5. **Copy the entire token** (it's very long!)

### 4️⃣ Configure the Widget

Open `Variables.inc` in Notepad and edit these lines:

```ini
HAHost=192.168.1.3          ← Your Home Assistant IP
HAPort=8123                 ← Usually 8123
HAToken=YOUR_TOKEN_HERE     ← Paste your token here
```

**Save the file!**

### 5️⃣ Load Widget in Rainmeter

1. Right-click **Rainmeter icon** in system tray
2. Click **Manage**
3. Under **Skins**, expand **AirQuality**
4. Click **AirQuality.ini**
5. Click **Load** button

✅ Done! Widget should appear on your desktop.

## ✅ Verification

You should see:
- PM values updating
- Gas concentrations showing
- AQI number with color
- "Connected" status at bottom

If you see "0" or "N/A":
- Check token is correct (no spaces, complete)
- Verify Home Assistant IP
- Test in browser: http://192.168.1.3:8123/api/states

## 🎨 Customize Position

- **Drag** widget to desired location
- **Right-click** → Settings → **Save position**

## 🔄 Updates

Widget refreshes every 30 seconds automatically (matches ESP32).

## 🆘 Problems?

See full troubleshooting in [`README.md`](README.md)

---

**That's it!** Your air quality data is now on your desktop.