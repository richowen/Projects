### Pin Assignments
```
// Active LOW relays
const int RELAY_AUGER = 25;
const int RELAY_AGITATOR = 26;
const int RELAY_MIXER = 16;
const int RELAY_WATER = 17;
const int LEVEL_SWITCH = 12;     // input pull-up, LOW=active
```
## System Operation

### Automatic Mixing Cycle
1) Trigger: Level switch active (LOW) -> start full cycle (auger+agitator+mixer+water).
2) When level restores (HIGH), transition to Post-Mix.
3) Post-Mix: mixer-only for 5 seconds.
4) Return to Idle.

### Periodic Mixing (Idle)
- Every 5 minutes, mixer-only for 5 seconds to keep milk stirred.

### ESP32 Board
- Board: FireBeetle ESP32
- Static IP: 192.168.1.16
- WiFi: SSID "WiFi", password "Gliders1!"