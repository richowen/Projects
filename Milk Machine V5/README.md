# Calf Feeding Machine V5

ESP32 (FireBeetle) firmware for an automated calf milk-powder mixing and
feeding machine. Controls four active-LOW relays driven by a single level
switch, with safety, reliability, and remote-monitoring features.

## Pin Assignments

```cpp
// Active LOW relays
const int RELAY_AUGER    = 25;
const int RELAY_AGITATOR = 26;
const int RELAY_MIXER    = 16;
const int RELAY_WATER    = 17;
// [Fix #7] GPIO27 — moved from GPIO12 (strap pin, unsuitable for general I/O)
const int LEVEL_SWITCH   = 27;   // input pull-up, LOW = active
```

## System Operation

### Automatic Mixing Cycle
1. Level switch LOW → start full mix cycle (auger + agitator + mixer + water).
2. Level switch HIGH → transition to Post-Mix.
3. Post-Mix: mixer-only for 5 seconds.
4. Return to Idle.

### Periodic Mixing (Idle)
- Every 5 minutes, mixer-only for 5 seconds to keep milk stirred when idle.

### Fault Handling
- Mixing exceeds 2 minutes → FAULT state, relays off, 60 s cooldown.
- Free heap below 8 KB → FAULT.
- 3 faults within 5 minutes → controlled `esp_restart()`.

## ESP32 Board

- Board: FireBeetle ESP32
- Static IP: 192.168.1.16
- WiFi credentials: defined in `src/secrets.h` (gitignored — copy from `src/secrets.h.template`)

## Remote Monitor

- TCP log stream: port 23 (password-protected)
- HTTP log viewer: port 80 (HTTP Basic auth)
- See `README_REMOTE_MONITOR.md` for full details.

## Credentials

Copy `src/secrets.h.template` to `src/secrets.h` and fill in values.
`secrets.h` is `.gitignored` and must never be committed.

## Testing

The safety-critical logic (state machine, fault handling, debouncer) is fully
exercised by a host-side Unity test suite — no hardware required. The logic
modules (`src/machine_core.*`, `src/level_debouncer.*`) are written with zero
Arduino/ESP dependencies so they build on a regular PC.

### Prerequisites
- A host C/C++ compiler on `PATH` (MinGW-w64 / gcc recommended on Windows).
  Install example: `winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT`
  then open a new terminal so the new `PATH` is picked up.

### Run all tests
```
pio test -e native
```

### Run one suite
```
pio test -e native -f test_machine_core
pio test -e native -f test_debouncer
pio test -e native -f test_faults
```

### What is covered (30 cases)
- **test_machine_core** (19): boot state, IDLE→MIXING→POST_MIX flow, POST_MIX
  duration, Fix #11 regression (no instant periodic after demand mix),
  PERIODIC_MIX firing/exit/escalation, mixing cap, stuck-LOW sensor,
  fault cooldown, 3-faults→reboot, fault-window expiry, low-heap fault,
  FAULT-always-off relay invariant, output caching, 32-bit `millis()`
  wraparound across 49.7 days in both MIXING and IDLE.
- **test_debouncer** (4): initial state, rejects short glitches, accepts
  changes after stable window, rapid toggling stays unstable.
- **test_faults** (7): back-to-back faults reach reboot, low-heap doesn't
  double-count, cooldown not short-circuited by demand, low-heap mid-
  periodic, exact reboot threshold, long-run fault-count bounds, sanity
  guard for runaway mixing.

All 30 tests must pass before firmware is uploaded to the machine.

