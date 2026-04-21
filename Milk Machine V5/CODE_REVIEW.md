# Code Review — Milk Machine V5

**Reviewed:** 2026-04-21  
**Files reviewed:** `src/main.cpp`, `src/remote_monitor.h`, `src/remote_monitor.cpp`, `src/monitor_config.h`, `platformio.ini`, `README.md`, `DEPLOYMENT.md`, `test_monitor.py`

---

## Summary Table

| # | Severity | Area | Issue |
|---|----------|------|-------|
| 1 | HIGH | Concurrency | `remoteMonitor.end()` called from WiFi event task while `loop()` uses the object |
| 2 | HIGH | Security | TCP server (port 23) has zero authentication — any LAN host gets the live log |
| 3 | MEDIUM | Memory | `String` append-and-trim on `logBuffer` fragments ESP32 heap over time |
| 4 | MEDIUM | Security | All credentials hardcoded in source — no `secrets.h` pattern |
| 5 | MEDIUM | Correctness | `#define MONITOR_ENABLE_AUTH true` — fragile preprocessor idiom, should be `1` |
| 6 | MEDIUM | Safety | `monitorHealth()` sanity-fault log bypasses `RLOG_*` — silent on remote monitor |
| 7 | MEDIUM | Docs | `README.md` & `DEPLOYMENT.md` reference stale `LEVEL_SWITCH = 12` (actual: 27) |
| 8 | MEDIUM | Docs | `DEPLOYMENT.md` says WiFi retries "every 30 seconds" — code uses exponential backoff |
| 9 | MEDIUM | Build | Unused `lib_deps`: `ESPAsyncWebServer-esphome`, `ArduinoJson`; unused `-D MQTT_MAX_PACKET_SIZE` |
| 10 | MEDIUM | Reliability | TCP dead-connection slots — no idle timeout, ghost conn blocks one of 2 slots indefinitely |
| 11 | LOW | Behaviour | POST_MIX -> IDLE does not reset `lastPeriodicMix` — can cause immediate periodic stir after full mix |
| 12 | LOW | Code quality | Duplicate constant: `WIFI_RETRY_INTERVAL` is just an alias of `WIFI_RETRY_INIT_MS`, unused |
| 13 | LOW | Code quality | `ArduinoOTA.begin()` called twice — boot + `GOT_IP` handler |
| 14 | LOW | Code quality | Unused/aspirational defines in `monitor_config.h` mislead maintainers |
| 15 | LOW | Memory | `remoteMonitor.printf()` stack buffer is 256 bytes — silently truncates longer messages |
| 16 | LOW | Reliability | OTA gated to `IDLE` only — stuck sensor could make OTA permanently unreachable |
| 17 | LOW | Reliability | `logBuffer` persists stale data through WiFi disconnect/reconnect cycles |
| 18 | LOW | Test script | `continuous_monitor()` missing `settimeout()` — hangs on silent server |
| 19 | LOW | Test script | `decode('utf-8')` with no `errors=` arg — crashes on any non-UTF8 byte from ESP32 |
| 20 | LOW | Test script | f-string formatting bug — `{MILK_MACHINE_IP}` literal printed instead of value |

---

## Recommended Fix Order

| Priority | Issue # | Action |
|----------|---------|--------|
| 1st | #1 | Move `begin()`/`end()` out of WiFi event handler (concurrency / use-after-free) |
| 2nd | #6 | Replace `Serial.println` in `monitorHealth()` with `RLOG_*` macros |
| 3rd | #7 | Correct LEVEL_SWITCH pin (12 -> 27) in README and DEPLOYMENT docs |
| 4th | #5 | Change `#define MONITOR_ENABLE_AUTH true` to `1` |
| 5th | #9 | Remove unused lib_deps and build flag from platformio.ini |
| 6th | #4 | Extract credentials to gitignored `secrets.h` |
| 7th | #3 | Replace String logBuffer with fixed circular char buffer (heap health) |
| 8th | #11 | Reset `lastPeriodicMix` on POST_MIX -> IDLE transition |
| 9th | #18-20 | Fix test_monitor.py bugs (timeout, decode, f-string) |
| Later | #2 | TCP port 23 authentication |
| Later | #16 | Allow OTA in FAULT state |
| Later | #10 | TCP idle timeout for ghost connections |

---

## Detailed Findings

---

### HIGH 1 — Concurrency: `remoteMonitor.end()` use-after-free risk

**Files:** `src/main.cpp:208`, `src/remote_monitor.cpp`

WiFi events fire on **Core 0** (the WiFi task). `loop()` runs on **Core 1**. Both touch `remoteMonitor`
concurrently without any mutex.

The dangerous path: WiFi drops -> `SYSTEM_EVENT_STA_DISCONNECTED` fires on Core 0 ->
`remoteMonitor.end()` runs, calls `delete webServer` and sets `webServer = nullptr`. At the exact same
moment, Core 1 is inside `remoteMonitor.handle()` calling `webServer->handleClient()`. This is a
use-after-free — likely a crash or silent memory corruption.

```cpp
// src/main.cpp:208 — runs on Core 0 (WiFi task)
case SYSTEM_EVENT_STA_DISCONNECTED:
    remoteMonitor.end();   // deletes webServer + tcpServer

// src/remote_monitor.cpp:67 — runs on Core 1 (loop task)
void RemoteMonitor::handle() {
    if (webServer) {              // non-atomic check-then-use
        webServer->handleClient(); // UAF if end() ran between check and here
    }
}
```

**Fix:** Move `begin()`/`end()` out of the WiFi event handler into the main loop. Use `volatile bool`
flags to signal intent from the event, and act on them inside `handleWiFiOTA()` where only Core 1 runs.

```cpp
// In main.cpp — replace onWiFiEvent with flag-only version:
volatile bool wifiJustConnected    = false;
volatile bool wifiJustDisconnected = false;

void onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            wifiJustConnected = true;
            wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifiJustDisconnected = true;
            break;
        default: break;
    }
}

// In handleWiFiOTA() — runs on Core 1 only:
if (wifiJustConnected) {
    wifiJustConnected = false;
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    ArduinoOTA.begin();
    if (!remoteMonitor.isEnabled())
        remoteMonitor.begin(MONITOR_TCP_PORT, MONITOR_WEB_PORT);
}
if (wifiJustDisconnected) {
    wifiJustDisconnected = false;
    Serial.println("WiFi disconnected.");
    remoteMonitor.end();
}
```

---

### HIGH 2 — Security: TCP port 23 has no authentication

**Files:** `src/remote_monitor.cpp:43-69`, `src/monitor_config.h`

`MONITOR_ENABLE_AUTH` only gates the **HTTP** routes (`handleRoot()` and `handleLogs()`). The TCP server
accepts any connecting client unconditionally and immediately streams live machine logs. Any other device
on `192.168.1.0/24` can passively monitor the machine.

**Fix options (pick one):**
- Send a password challenge on TCP connect; disconnect if not answered within 5 s.
- Accept the risk deliberately and add a comment documenting the decision.

---

### MEDIUM 3 — Memory: `String` heap fragmentation in `RemoteMonitor`

**Files:** `src/remote_monitor.cpp:88-113`, `handleRoot()`

The `logBuffer` `String` grows with `+=` and is trimmed with `substring()`. On every trim, `substring()`
allocates a new `String`, the old one is freed, and the new one is assigned. Over thousands of loop
iterations this creates alternating alloc/free patterns that fragment the ESP32 heap. The 8 KB heap guard
provides a safety net, but fragmentation will silently degrade the largest contiguous free block well
before total free heap drops below 8 KB.

`handleRoot()` also builds the HTML response with 10+ `+=` operations, each of which may reallocate.

**Fix:** Replace `logBuffer String` with a fixed `char` circular buffer:

```cpp
// In RemoteMonitor (remote_monitor.h):
static constexpr size_t LOG_BUF_SIZE = MONITOR_LOG_BUFFER_SIZE;
char   logBuf[LOG_BUF_SIZE];
size_t logHead = 0;  // oldest byte index
size_t logLen  = 0;  // bytes currently stored

void appendLog(const char* s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        logBuf[(logHead + logLen) % LOG_BUF_SIZE] = s[i];
        if (logLen < LOG_BUF_SIZE) logLen++;
        else logHead = (logHead + 1) % LOG_BUF_SIZE;
    }
}
```

For the HTML response use chunked transfer to avoid a single large allocation:
```cpp
webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
webServer->sendHeader("Content-Type", "text/html");
webServer->sendContent(staticHtmlHeader);
webServer->sendContent(logBufAsString);
webServer->sendContent(staticHtmlFooter);
webServer->client().stop();
```

---

### MEDIUM 4 — Security: Credentials hardcoded in source

**Files:** `src/main.cpp:20-21`, `src/main.cpp:246`, `src/monitor_config.h:10`, `test_monitor.py:13`

All four credential strings (`ssid`, WiFi `password`, OTA `password`, `MONITOR_PASSWORD`) are identical
(`"Gliders1!"`) and appear in plain source. Anyone with repository access can read them.

**Fix:** Extract to a `src/secrets.h` and add it to `.gitignore`:

```cpp
// src/secrets.h  <- add to .gitignore
#pragma once
#define SECRET_WIFI_SSID     "WiFi"
#define SECRET_WIFI_PASSWORD "Gliders1!"
#define SECRET_OTA_PASSWORD  "Gliders1!"
#define SECRET_MONITOR_PASS  "Gliders1!"
```

Add `src/secrets.h.template` to the repo with placeholder values so a new developer knows what the file
should look like.

---

### MEDIUM 5 — Correctness: `#define MONITOR_ENABLE_AUTH true` — fragile preprocessor

**File:** `src/monitor_config.h:9`

`#if MONITOR_ENABLE_AUTH` evaluates the identifier `true` in the preprocessor. If `<stdbool.h>` is not
yet included, `true` is unknown and expands to `0`, silently **disabling auth**. It works today because
`<WiFi.h>` happens to pull in `<Arduino.h>` -> `<stdbool.h>` first, but this is an implicit ordering
dependency.

**Fix:**
```cpp
#define MONITOR_ENABLE_AUTH 1   // 1 = enabled, 0 = disabled
```

---

### MEDIUM 6 — Safety: Sanity-fault logs in `monitorHealth()` bypass remote monitor

**File:** `src/main.cpp:391-399`

```cpp
// Both use Serial.printf / Serial.println — NOT RLOG_* macros:
Serial.printf("CRITICAL: low heap %u bytes < %u threshold\n", ...);
// ...
Serial.println("Sanity: MIXING exceeded safety+margin -> entering fault");
```

A remote observer via TCP or HTTP gets no indication of these critical safety events — the exact moments
the remote monitor is most needed.

**Fix:**
```cpp
RLOG_PRINTF("CRITICAL: low heap %u bytes < %u threshold\n",
            (unsigned)freeHeap, (unsigned)MIN_SAFE_HEAP);
// ...
RLOG_PRINTLN("Sanity: MIXING exceeded safety+margin -> entering fault");
```

---

### MEDIUM 7 — Docs: Stale `LEVEL_SWITCH` pin in README and DEPLOYMENT

**Files:** `README.md:8`, `DEPLOYMENT.md` (checklist item 4, on-site step 4, troubleshooting section)

Firmware uses **GPIO27**. Both docs still say **GPIO12**. GPIO12 is a strap pin that affects flash boot
mode — wiring a sensor there on a replacement unit could cause boot failures.

**Fix:** Replace all occurrences of `pin 12` / `GPIO12` / `LEVEL_SWITCH = 12` with `GPIO27` across both
files.

---

### MEDIUM 8 — Docs: WiFi reconnect description is inaccurate

**File:** `DEPLOYMENT.md`, section "Network Connectivity Loss"

> "System attempts WiFi reconnection every 30 seconds"

Actual behaviour: initial retry at 30 s, doubles each attempt up to a maximum of 5 minutes. A maintainer
expecting 30-second retries will be confused after a prolonged outage.

**Fix:**
```
System attempts WiFi reconnection with exponential backoff: first retry at 30 s,
doubling each attempt up to a maximum of 5 minutes between retries.
Backoff resets immediately to 30 s on successful connection.
```

---

### MEDIUM 9 — Build: Three unused dependencies / flags

**File:** `platformio.ini`

| Entry | Issue |
|-------|-------|
| `esphome/ESPAsyncWebServer-esphome@^3.1.0` | Zero includes in any source file; pulls in AsyncTCP |
| `bblanchon/ArduinoJson@^6.17.2` | Zero includes in any source file |
| `-D MQTT_MAX_PACKET_SIZE=512` | No MQTT code anywhere; orphan from a previous project template |

**Fix:** Remove all three. If ESPAsyncWebServer is pre-staged for a future SSE/WS upgrade, keep it
commented out with an explanatory note:

```ini
; Pre-staged for future SSE/WebSocket upgrade — uncomment when implementing:
; esphome/ESPAsyncWebServer-esphome@^3.1.0
```

---

### MEDIUM 10 — Reliability: TCP ghost connections block new clients

**File:** `src/remote_monitor.cpp:43-70`

With `MONITOR_MAX_CLIENTS = 2`, a client that loses power without sending a TCP FIN/RST holds its slot
until the TCP stack detects the dead peer (can take several minutes). During that window one ghost
connection already halves capacity; two ghosts block all new connections.

**Fix:** Track last-activity timestamp per client and force-close after an idle timeout:

```cpp
// Add to RemoteMonitor private members:
unsigned long lastClientActivity[MONITOR_MAX_CLIENTS] = {};
static constexpr unsigned long CLIENT_IDLE_TIMEOUT_MS = 5UL * 60UL * 1000UL;

// In handle(), before new-client logic:
unsigned long now = millis();
for (int i = 0; i < MONITOR_MAX_CLIENTS; i++) {
    if (tcpClients[i] && tcpClients[i].connected()) {
        if ((now - lastClientActivity[i]) > CLIENT_IDLE_TIMEOUT_MS) {
            tcpClients[i].stop();
        }
    }
}
// Reset lastClientActivity[i] = millis() when a client connects or data is sent.
```

---

### LOW 11 — Behaviour: Immediate periodic mix after a demand-mix cycle

**File:** `src/main.cpp:358-365` (POST_MIX -> IDLE transition)

`lastPeriodicMix` is only updated when entering `PERIODIC_MIX`. If a demand mix completes when
`lastPeriodicMix` is already older than `PERIODIC_MIX_INTERVAL`, the machine immediately fires a
periodic stir on returning to IDLE — running the mixer for an extra 5 seconds right after a full mix.

**Fix:** Reset `lastPeriodicMix` in the POST_MIX -> IDLE transition:

```cpp
case POST_MIX:
    if (inStateFor(POST_MIX_DURATION)) {
        RLOG_PRINTLN("STATE: POST_MIX -> IDLE");
        allRelaysOff();
        lastPeriodicMix = millis();   // prevent immediate periodic stir
        transitionTo(IDLE);
        return;
    }
    break;
```

---

### LOW 12 — Code quality: Dead constant `WIFI_RETRY_INTERVAL`

**File:** `src/main.cpp:62-65`

```cpp
const unsigned long WIFI_RETRY_INIT_MS  = 30UL * 1000;
const unsigned long WIFI_RETRY_INTERVAL = WIFI_RETRY_INIT_MS;  // never used
```

`WIFI_RETRY_INTERVAL` is not referenced anywhere. Remove it.

---

### LOW 13 — Code quality: `ArduinoOTA.begin()` called twice

**Files:** `src/main.cpp:259`, `src/main.cpp:205`

`begin()` is called once in `initializeOTA()` at boot and again in `onWiFiEvent(GOT_IP)`. Harmless in
practice but confusing. If fixing issue #1 (moving event-handler logic to `handleWiFiOTA()`), this
naturally collapses to one call.

---

### LOW 14 — Code quality: Aspirational defines in `monitor_config.h` not implemented

**File:** `src/monitor_config.h:17-21`

```cpp
#define MONITOR_AUTO_RESTART_ON_HANG true
#define MONITOR_RESTART_DELAY_MS 5000
#define MONITOR_MAX_CONNECTIONS_PER_MINUTE 10
#define MONITOR_ENABLE_RATE_LIMITING true
```

These are defined but never read anywhere. A maintainer might assume they are already active. Either
implement them or move them to a `// TODO` comment block.

---

### LOW 15 — Memory: `printf` buffer silent truncation at 256 bytes

**File:** `src/remote_monitor.cpp:88`

```cpp
char buffer[256];
vsnprintf(buffer, sizeof(buffer), format, args);
```

`vsnprintf` silently truncates messages longer than 255 characters. Current log messages are well under
this limit. At minimum, detect truncation:

```cpp
int n = vsnprintf(buffer, sizeof(buffer), format, args);
if (n >= (int)sizeof(buffer)) {
    strlcat(buffer, "...[trunc]", sizeof(buffer));
}
```

---

### LOW 16 — Reliability: OTA permanently unreachable with stuck level sensor

**File:** `src/main.cpp:414-416`

If the level switch fails closed (stuck LOW), the machine cycles: IDLE -> MIXING -> FAULT (2 min cap) ->
IDLE (briefly) -> MIXING again. OTA only runs in IDLE, which is visited for milliseconds before the next
MIXING transition. A firmware update to fix the sensor handling becomes effectively impossible without a
physical power cycle.

**Fix options:**
- Add a boot window (e.g., first 30 s after reset) where OTA is serviced before the state machine
  activates. Requires holding relays off for that window.
- Allow OTA in FAULT state — relays are already forced off on FAULT entry, making it OTA-safe.

---

### LOW 17 — Reliability: Log buffer not cleared between reconnects

**File:** `src/remote_monitor.cpp` (`end()`)

`end()` does not clear `logBuffer`. After a WiFi drop and reconnect, a new HTTP client sees all logs
from the previous session. This may be desired (log continuity) or undesired (stale data displayed as
current). Add an explicit decision in the code:

```cpp
void RemoteMonitor::end() {
    // ...
    // Uncomment to clear history on disconnect:
    // logBuffer = "";
    enabled = false;
}
```

---

### LOW 18 — Test script: `continuous_monitor()` hangs on silent server

**File:** `test_monitor.py:~65`

`sock.recv(1024)` blocks indefinitely if the ESP32 stops sending. Add a timeout:

```python
sock.settimeout(5.0)
# then in the loop, catch socket.timeout and continue
```

---

### LOW 19 — Test script: `decode('utf-8')` crashes on bad bytes

**File:** `test_monitor.py:29, 70`

If the ESP32 resets mid-transmission, garbled bytes cause `UnicodeDecodeError` and crash the script.

**Fix:**
```python
data = sock.recv(1024).decode('utf-8', errors='replace')
```

---

### LOW 20 — Test script: f-string formatting bug

**File:** `test_monitor.py:~85`

```python
print("Access web interface at: http://{MILK_MACHINE_IP}/")
# Prints literally: Access web interface at: http://{MILK_MACHINE_IP}/
```

Missing `f` prefix — `{MILK_MACHINE_IP}` is never substituted.

**Fix:**
```python
print(f"Access web interface at: http://{MILK_MACHINE_IP}/")
```

---

## Round 2 Review — 2026-04-21

After applying all 20 fixes above, a second pass found the following additional issues. All are now fixed.

| # | Severity | Area | Issue | Status |
|---|----------|------|-------|--------|
| A | MEDIUM | Correctness | `sendLogToClient` never sent `\n` — telnet clients saw all log lines concatenated on one row | FIXED |
| B | MEDIUM | Reliability | `WiFiClient::print()` can block up to 1 s per slow client — up to N×timeout loop stall risk | FIXED |
| C | MEDIUM | Memory | `handleRoot()` allocated ~10 KB transient String per HTTP request (the exact churn #3 was meant to eliminate) | FIXED |
| D | LOW | Performance | `appendLog()` per-byte loop with `%` — replaced by up-to-2 `memcpy` chunks | FIXED |
| E | LOW | Correctness | `printf()` entries could merge in log buffer when caller omitted `\n` | FIXED |
| F | LOW | Code quality | `wifiRetryIntervalMs` reset duplicated across connect-branch and connected-iteration branch | FIXED |
| G | INFO | Readability | `presetOutputHigh()` write-before-pinMode looks like a bug — now commented explaining intent | FIXED |

### Fix A/E — Unified `writeRecord()` in RemoteMonitor
Both `println()` and `printf()` now route through a single `writeRecord(msg, len)` which:
- Strips trailing `\r`/`\n` from the caller's message (idempotent — callers can still pass them)
- Appends exactly one `\n` to the log buffer
- Writes `msg + "\r\n"` to each authenticated TCP client

### Fix B — Non-blocking TCP writes
On client accept:
```cpp
tcpClients[i].setNoDelay(true);
tcpClients[i].setTimeout(50);
```
And in `writeRecord()`:
```cpp
if (tcpClients[i].availableForWrite() < (int)(tsLen + msgLen + 2)) continue;
```
Logs addressed to a stuck peer are silently dropped instead of blocking the loop task.

### Fix C — Chunked HTTP output, no intermediate String
`handleRoot()` uses `CONTENT_LENGTH_UNKNOWN` chunked transfer, sends `PROGMEM` HTML fragments with `sendContent_P()`, and streams the circular log buffer via a 256-byte `streamLogEscaped()` that HTML-escapes inline.
`handleLogs()` sets `Content-Length: logLen` and sends the two contiguous circular-buffer slices directly — zero extra allocations.
Transient heap per HTTP request dropped from ~10 KB to <300 B.

### Fix D — `appendLog()` memcpy rewrite
Replaces the byte-at-a-time loop with up to two `memcpy()` calls. Also handles the pathological `len >= LOG_BUF_SIZE` case by keeping only the trailing `LOG_BUF_SIZE` bytes.

### Fix F — Removed duplicate backoff reset in `handleWiFiOTA()`
`wifiRetryIntervalMs = WIFI_RETRY_INIT_MS;` is now set once per iteration only in the "connected" branch.

### Build verification
```
RAM:   10.0% (53376 / 532480 bytes)
Flash: 63.4% (830585 / 1310720 bytes)
[SUCCESS] Took 16.89 seconds
```

---

## Round 3 — Hardening for 100% uptime

The goal of Round 3 was to take the safety-critical logic out of `main.cpp`
(where it was intertwined with Arduino / WiFi / OTA lifecycle) and put it
behind a host-testable boundary, then drive that boundary from a full Unity
test suite. All hardware pins are still owned by `main.cpp`, but every state
transition decision is now made by a pure C++17 class.

### Refactor
- **New module `src/machine_core.{h,cpp}`** — pure state machine.
  - `enum class SysState { IDLE, MIXING, POST_MIX, PERIODIC_MIX, FAULT }`
  - `struct MachineInputs  { bool levelLow; uint32_t nowMs; size_t freeHeap; }`
  - `struct MachineOutputs { bool auger, agitator, mixer, water; bool rebootRequested; }`
  - `struct MachineConfig` holds all tuneables (cap, cooldown, interval, window,
    margin, maxFaults, minSafeHeap) — defaults mirror previous behaviour.
  - `class Machine` with `reset(now)`, `tick(in,out)`, `state()`, `faultCount()`,
    `lastEvent()`, labelled `stateStr()` / `eventStr()` for logging/tests.
  - **Invariant:** `enterFault_()` always calls `writeRelays_(0,0,0,0)` before
    the transition — every path into FAULT leaves the relays off.
  - **Wrap-safe timing:** all elapsed calculations use unsigned 32-bit
    subtraction so 49.7-day `millis()` wraparound is handled automatically.
  - Zero Arduino/ESP deps — compiles on the host and inside the firmware.
- **New module `src/level_debouncer.{h,cpp}`** — 50 ms debounce FSM, pure logic.
- **Rewrote `src/main.cpp`** as thin glue (~230 lines). Reads debounced level,
  calls `machine.tick()`, forwards `out` through cached relay writes, honours
  `out.rebootRequested` via `esp_restart()`, logs transitions via `RLOG_*`.

### Test suite — 30 cases, all passing
```
pio test -e native
```

| Suite               | Cases | Focus |
|---------------------|-------|-------|
| test_machine_core   | 19    | Happy paths, transitions, periodic timer, 49.7-day wrap, labels |
| test_debouncer      | 4     | Initial state, short-glitch rejection, stable transition, rapid toggle |
| test_faults         | 7     | Back-to-back faults, FAULT during low heap, cooldown not short-circuited, exact reboot threshold, sanity guard |

Result (2026-04-21):
```
30 test cases: 30 succeeded
```

Notable regression coverage:
- **Fix #11** — `test_postmix_to_idle_does_not_trigger_periodic_immediately`
  guards the bug where a demand mix ending with a ripe periodic timer would
  immediately start a 5 s stir.
- **49.7-day `millis()` wrap** — `test_millis_wrap_during_mixing_still_faults`
  and `test_millis_wrap_during_idle_periodic_timer_works` prove both the
  safety cap and the periodic scheduler survive the rollover.
- **FAULT-relays-off invariant** — asserted on every path that can enter
  FAULT (cap, low heap, sanity guard).

### Build verification
- Firmware (`env:firebeetle32`): RAM 10.0% (53408 B), Flash 63.3% (830001 B) — unchanged ballpark.
- Host tests (`env:native`): 30/30 passing on MinGW-w64 gcc 15.2.0.

### Developer workflow
1. Change logic only in `machine_core.*` or `level_debouncer.*`.
2. `pio test -e native` must show `30 test cases: 30 succeeded`.
3. `pio run` must compile `env:firebeetle32` cleanly.
4. Upload OTA.

