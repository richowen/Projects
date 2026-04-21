# Calf Feeding Machine V5 - Deployment Guide

## Production Deployment Strategy

### Pre-Deployment Checklist

1. **Hardware Verification**
   - [ ] ESP32 FireBeetle board functioning
   - [ ] All 4 relays wired to correct pins (25, 26, 16, 17)
   - [ ] Level switch connected to **GPIO27** with pull-up  <!-- [Fix #7] was GPIO12 -->
   - [ ] Power supply stable and reliable
   - [ ] All connections secured and protected from moisture

2. **Credentials**
   - [ ] `src/secrets.h` created from `src/secrets.h.template` with correct values
   - [ ] `secrets.h` is NOT committed to version control

3. **Network Configuration**
   - [ ] WiFi network available at installation site
   - [ ] Static IP 192.168.1.16 available and not conflicting
   - [ ] Router configured to allow OTA on port 3232

4. **Software Preparation**
   - [ ] Code compiled successfully in PlatformIO
   - [ ] Upload via serial initially (not OTA for first deployment)
   - [ ] Serial monitor tested and working

### Deployment Steps

#### Initial Installation

1. **Connect ESP32 to development computer via USB**
2. **Copy `src/secrets.h.template` to `src/secrets.h` and fill in credentials**
3. **Open project in PlatformIO (VSCode extension)**
4. **Build and upload firmware:**
   ```
   PlatformIO: Build  (Ctrl+Alt+B)
   PlatformIO: Upload (Ctrl+Alt+U)
   ```
5. **Verify operation via Serial Monitor (115200 baud)**
6. **Test level switch operation manually**
7. **Verify relay operation (listen for relay clicks)**
8. **Confirm WiFi connection and OTA readiness**

#### On-Site Installation

1. **Power down all equipment**
2. **Install ESP32 in weatherproof enclosure**
3. **Connect relay outputs to milk machine components:**
   - Pin 25 → Auger control relay
   - Pin 26 → Agitator control relay
   - Pin 16 → Mixer control relay
   - Pin 17 → Water control relay
4. **Connect level switch to GPIO27**  <!-- [Fix #7] was GPIO12 -->
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

#### Remote Monitoring

Connect to TCP port 23 (password required) or browse to http://192.168.1.16 (HTTP Basic auth).
See `README_REMOTE_MONITOR.md` for full details.

#### Sample serial / remote log output

```
=== System Status ===
State: IDLE
Level Switch: HIGH (milk sufficient)
WiFi: Connected
Uptime: 86400 s
Free Heap: 245760 bytes
====================
```

#### OTA Updates

**Prerequisites:**
- ESP32 connected to WiFi network
- System in IDLE or FAULT state (automatic safety — relays are OFF in both)
- Development computer on same network

**Update Process:**
1. Modify code as needed
2. Build project in PlatformIO
3. Use OTA upload (`upload_protocol = espota` in platformio.ini)
4. Monitor serial output during update
5. Verify new firmware operation

### Troubleshooting Guide

#### System Not Responding
- Check power supply voltage and stability
- Verify all ground connections
- Reset ESP32 manually (watchdog auto-restarts after 10 s)
- Check serial output for error messages

#### Relays Not Activating
- Verify relay board power supply
- Check signal wire connections to ESP32
- Test relay board separately
- Monitor serial output during mixing cycle

#### Level Switch Issues
- Clean sensor of milk residue
- Check wiring and connection to **GPIO27**  <!-- [Fix #7] was GPIO12 -->
- Verify pull-up functionality (pin should read HIGH with nothing connected)
- Test switch manually with multimeter

#### WiFi / OTA Problems
- Check network availability at installation site
- Verify router allows connections on ports 3232 (OTA), 23 (TCP monitor), 80 (HTTP monitor)
- Confirm static IP not conflicting
- Core mixing operation continues regardless of WiFi state

#### WiFi Reconnection Behaviour  <!-- [Fix #8] -->
The firmware uses **exponential backoff** for reconnect attempts:
- First retry: 30 seconds after disconnect
- Each subsequent retry doubles the interval
- Maximum retry interval: 5 minutes
- Backoff resets to 30 seconds immediately on successful connection

#### Memory or Performance Issues
- Monitor free heap in serial output (fault threshold: 8 KB)
- Heap should remain stable; a downward trend indicates a leak
- System auto-restarts after 3 faults within 5 minutes

### Emergency Procedures

#### Complete System Failure
1. System fails to respond to level switch
2. Power cycle ESP32 (disconnect power for 10 seconds)
3. If problem persists, deploy backup ESP32 with known good firmware
4. Core mixing priority: manual relay control if needed

#### Network Connectivity Loss
- Core mixing operation continues normally without WiFi
- OTA updates unavailable until connectivity restored
- System attempts WiFi reconnect with exponential backoff (see above)
- No impact on milk production reliability

### Backup and Recovery

#### Firmware Backup
- Keep working firmware binary in a secure location
- Document any custom configuration changes in `src/secrets.h`
- Maintain a spare ESP32 with identical firmware

#### Configuration Backup
- Network settings documented in this file
- Pin assignments documented in `README.md` and `src/main.cpp`
- Credentials stored only in `src/secrets.h` (NOT in version control)

## Success Criteria

- **Core Function**: Level switch LOW → mix milk → level switch HIGH → stop
- **Reliability**: System operates continuously without intervention
- **Safety**: All relays OFF on power-up and error conditions
- **Maintenance**: OTA updates possible for remote firmware changes
- **Monitoring**: Remote TCP/HTTP monitor provides live status and logs

## Production Notes

- Hardware watchdog prevents system lockups (10 s timeout)
- All timing critical for milk quality and safety
- WiFi/OTA failures do not affect core mixing operation
- Active-LOW relays ensure safe state on power loss
- Debounced level switch prevents false triggers (50 ms window)
- Non-blocking loop ensures immediate level switch response
- OTA is only serviced when relays are guaranteed OFF (IDLE or FAULT state)
