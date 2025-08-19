# Calf Feeding Machine V5 - Deployment Guide

## Production Deployment Strategy

### Pre-Deployment Checklist

1. **Hardware Verification**
   - [ ] ESP32 FireBeetle board functioning
   - [ ] All 4 relays wired to correct pins (25, 26, 16, 17)
   - [ ] Level switch connected to pin 12 with pull-up
   - [ ] Power supply stable and reliable
   - [ ] All connections secured and protected from moisture

2. **Network Configuration**
   - [ ] WiFi network "WiFi" available at installation site
   - [ ] Static IP 192.168.1.16 available and not conflicting
   - [ ] Router configured to allow OTA on port 3232

3. **Software Preparation**
   - [ ] Code compiled successfully in PlatformIO
   - [ ] Upload via serial initially (not OTA for first deployment)
   - [ ] Serial monitor tested and working

### Deployment Steps

#### Initial Installation

1. **Connect ESP32 to development computer via USB**
2. **Open project in PlatformIO (VSCode extension)**
3. **Build and upload firmware:**
   ```
   PlatformIO: Build (Ctrl+Alt+B)
   PlatformIO: Upload (Ctrl+Alt+U)
   ```
4. **Verify operation via Serial Monitor (115200 baud)**
5. **Test level switch operation manually**
6. **Verify relay operation (listen for relay clicks)**
7. **Confirm WiFi connection and OTA readiness**

#### On-Site Installation

1. **Power down all equipment**
2. **Install ESP32 in weatherproof enclosure**
3. **Connect relay outputs to milk machine components:**
   - Pin 25 → Auger control relay
   - Pin 26 → Agitator control relay  
   - Pin 16 → Mixer control relay
   - Pin 17 → Water control relay
4. **Connect level switch to pin 12**
5. **Apply power and verify startup sequence**
6. **Test complete mixing cycle**

### Monitoring and Maintenance

#### System Health Checks

**Daily (Automatic)**
- Watchdog timer ensures system reset if frozen
- Level switch responds to milk level changes
- Periodic mixing every 5 minutes maintains milk quality

**Weekly (Manual)**
- Check serial output for any error messages
- Verify WiFi connection status
- Test manual level switch trigger
- Confirm all relays activate during mixing cycle

**Monthly (Maintenance)**
- Clean level switch sensor
- Check relay contacts for wear
- Verify static IP connectivity
- Test OTA update capability

#### Remote Monitoring via Serial

Connect to serial port (115200 baud) to monitor:
```
=== System Status ===
State: IDLE
Level Switch: HIGH (milk sufficient)  
WiFi: Connected
Uptime: 86400 seconds
Free Heap: 245760 bytes
====================
```

#### OTA Updates

**Prerequisites:**
- ESP32 connected to "WiFi" network
- System in IDLE state (automatic safety feature)
- Development computer on same network

**Update Process:**
1. **Modify code as needed**
2. **Build project in PlatformIO**
3. **Use OTA upload:**
   ```
   PlatformIO: Upload (make sure upload_protocol = espota)
   ```
4. **Monitor serial output during update**
5. **Verify new firmware operation**

### Troubleshooting Guide

#### System Not Responding
- **Check power supply voltage and stability**
- **Verify all ground connections**
- **Reset ESP32 manually (watchdog should auto-reset)**
- **Check serial output for error messages**

#### Relays Not Activating
- **Verify relay board power supply**
- **Check signal wire connections to ESP32**
- **Test relay board separately**
- **Monitor serial output during mixing cycle**

#### Level Switch Issues
- **Clean sensor of milk residue**
- **Check wiring and connection to pin 12**
- **Verify pull-up resistor functionality**
- **Test switch manually with multimeter**

#### WiFi/OTA Problems
- **Check network availability at installation site**
- **Verify router allows connection on port 3232**
- **Confirm static IP not conflicting**
- **Core mixing operation continues regardless**

#### Memory or Performance Issues
- **Monitor free heap in serial output**
- **Check for memory leaks (heap should be stable)**
- **Verify watchdog timer is resetting properly**
- **System auto-restarts every ~10 seconds if code hangs**

### Emergency Procedures

#### Complete System Failure
1. **System fails to respond to level switch**
2. **Power cycle ESP32 (disconnect power for 10 seconds)**
3. **If problem persists, deploy backup ESP32 with known good firmware**
4. **Core mixing priority: manual relay control if needed**

#### Network Connectivity Loss
- **Core mixing operation continues normally**
- **OTA updates unavailable until connectivity restored**
- **System attempts WiFi reconnection every 30 seconds**
- **No impact on milk production reliability**

### Backup and Recovery

#### Firmware Backup
- **Keep working firmware binary in secure location**
- **Document any custom configuration changes**
- **Maintain spare ESP32 with identical firmware**

#### Configuration Backup
- **Network settings documented in this file**
- **Pin assignments documented in README.md**
- **No user configuration stored on ESP32**

## Success Criteria

✅ **Core Function**: Level switch LOW → mix milk → level switch HIGH → stop  
✅ **Reliability**: System operates continuously without intervention  
✅ **Safety**: All relays OFF on power-up and error conditions  
✅ **Maintenance**: OTA updates possible for remote firmware changes  
✅ **Monitoring**: Serial output provides system status and debugging  

## Production Notes

- **Hardware watchdog prevents system lockups**
- **All timing critical for milk quality and safety**
- **WiFi/OTA failures do not affect core operation**
- **Active-LOW relays ensure safe state on power loss**
- **Debounced level switch prevents false triggers**
- **Non-blocking code ensures immediate level switch response**