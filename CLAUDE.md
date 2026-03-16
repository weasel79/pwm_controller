# Universal PWM Control (pwm_controller)

## Project Overview
ESP32 + PCA9685 16-channel PWM output controller with web UI, PS4 controller input, analog/digital inputs, envelope/sequence playback, and preset system.

- **Board**: esp32dev (Arduino framework, PlatformIO)
- **PCA9685** 16-channel PWM driver over I2C (SDA=21, SCL=22)
- **WiFi** STA mode (SSID: blndr), ESPAsyncWebServer on port 80
- **PS4 controller** via Bluetooth Classic (aed3/PS4-esp32 library)
- **Partition**: huge_app.csv

## Architecture
- `main.cpp` — setup/loop, wires all modules together
- `output_controller.h/.cpp` — PCA9685 driver, channel state, curve playback
- `wifi_controller.h/.cpp` — WiFi init, ESPAsyncWebServer REST API
- `web_ui.h` — Full HTML/CSS/JS web UI as C++ raw string literal
- `ps4_input.h/.cpp` — PS4 BT Classic input, caches state in PS4State struct
- `analog_input.h/.cpp` — ADC pot inputs with smoothing/deadzone
- `digital_input.h/.cpp` — GPIO digital inputs with debounce/toggle/latch
- `config.h` — All pin definitions, WiFi credentials, timing constants
- `patch_ps4lib.py` — Pre-build script that patches PS4Controller library

## Build
```bash
pio run                    # compile
pio run -t upload          # flash
pio device monitor         # serial at 115200
```
- **upload_speed: max 115200** — higher speeds cause upload failures on ESP32
- RAM: ~17.9%, Flash: ~54.5%

## Critical Gotchas

### ESPAsyncWebServer Route Prefix Matching
ESPAsyncWebServer v1.2.4 does prefix matching on POST routes with body handlers.
`/api/output/input` gets caught by `/api/output` handler silently.
**Rule**: Never nest routes under existing route paths.

### PS4 Controller Crash (host_recv_pkt_cb assert)
PS4Controller library has `delay(250)` in `_connection_callback` that blocks the Bluedroid task. During the delay, PS4 sends ~62 packets and the HCI receive buffer overflows.
**Fix**: `patch_ps4lib.py` removes the delay automatically via `extra_scripts = pre:patch_ps4lib.py`.

### PS4 Thread Safety
Never call PS4 library functions from the AsyncWebServer task (different FreeRTOS task). PS4 state is cached in `PS4State` struct, updated only in `loop()`, and read by the web server via `ps4->getState()`.

### WiFi/BT Coexistence
- Must use `ESP_COEX_PREFER_BALANCE` (not PREFER_WIFI, not PREFER_BT)
- `ESP_COEX_PREFER_BT` causes `abort()` at boot on this ESP-IDF version
- `WiFi.setSleep(false)` causes `abort()` in STA mode — only safe in AP mode

### Stack Size
Default loop stack (8192) is required. `-DARDUINO_LOOP_STACK_SIZE=4096` causes crash during WiFi+Bluedroid init.

## Memory Budget
- Free heap after boot: ~54KB (WiFi + PS4 BT Classic + PCA9685 + LittleFS)
- Sequence buffer allocated on-demand (saves ~60KB when idle)

## REST API Routes
| Method | Route | Description |
|--------|-------|-------------|
| GET | `/api/outputs` | All channel states |
| POST | `/api/output` | Set single channel value |
| POST | `/api/input` | Set channel input source |
| POST | `/api/output/type` | Set channel output type |
| POST | `/api/pwm-range` | Set PWM tick min/max |
| POST | `/api/pot-assign` | Assign pot to channel |
| POST | `/api/digital-config` | Set digital pin/mode |
| GET | `/api/raw-inputs` | PS4/analog/digital values |
| POST | `/api/curve-play` | Start curve playback |
| POST | `/api/curve-stop` | Stop curve playback |
| POST | `/api/stop-all` | Stop all curves |
| POST | `/api/global-freq` | Set PWM frequency |
| GET/POST | `/api/seq-*` | Sequence save/load/list/del |
| GET/POST | `/api/env-*` | Envelope save/load/list/del |
| GET/POST | `/api/preset-*` | Preset save/load/list/del |
| POST | `/api/ota` | Firmware upload |

---

## Session — 2026-02-27 — PS4 stability fixes, slider flow control, ELRS/scripting/Blockly roadmap

### Summary
- Fixed PS4 controller crash under sustained WiFi+BT load (smart polling, flow control)
- Fixed slider lag from AsyncTCP rx/ack timeouts during rapid fader movement
- Created new sister project esp32-mouldking-test (MouldKing BLE motor control)
- Assessed three roadmap features: ELRS RC input, embedded scripting, Blockly visual programming
- Wrote `roadmap assessment.md` with full technical analysis

### Key findings
- `ESP_COEX_PREFER_BT` causes `abort()` at boot on ESP-IDF 4.4 — only `PREFER_BALANCE` works
- `WiFi.setSleep(false)` causes `abort()` in STA mode — only safe in AP mode
- Slider lag root cause: 30ms throttle fires new fetch while previous still in-flight → TCP pileup
- Fix: flow control flag (`sliderSending`) ensures max 1 HTTP request in-flight at a time
- Smart polling: skip `/api/raw-inputs` when PS4 connected (data routing is ESP-side), only fetch `/api/outputs`
- ELRS receiver: CRSF serial on UART2 (GPIO 16/17), 420K baud, AlfredoCRSF library, ~2KB flash
- RadioMaster Pocket: 4 gimbals + 5 switches + 1 pot + 6 configurable = 16 channels
- MyBasic scripting: ~10-15KB heap, runs in FreeRTOS task, good fit for 54KB free heap
- Blockly from CDN: ~0KB ESP32 flash, generates JSON action sequences for ESP32 execution

### Issues
- PS4 may still crash under very sustained load (~2+ min) — smart polling reduces but may not eliminate
- `roadmap assessment.md` not yet committed to git

### To-do
- Phase 1: Implement ELRS input (~4-6h) — plan written, ready to start
- Phase 2: Implement scripting engine (~8-12h)
- Phase 3: Implement Blockly visual editor (~12-16h)

### Ideas
- ELRS + Blockly synergy: "Wait until ELRS CH5 > 128" blocks for RC-triggered sequences
- Blockly generates MyBasic/Lua scripts instead of JSON for unified backend

---

## Session — 2026-03-15 — MouldKing BLE integration, PS4 fix, TCP optimization

### Summary
Integrated MouldKing 6.0 BLE motor control (6 channels A-F) into the PWM controller.
MK channels appear as virtual outputs 16-21 in the web UI with full input routing
(manual, envelope, sequence, pot, PS4, digital). Fixed PS4 controller not connecting
after MK integration. Addressed AsyncTCP socket exhaustion causing web UI freezes.
Added mDNS (http://pwm.local). Renamed project from servo-geloet-esp32.

### Key findings
- **NimBLE + Bluedroid conflict**: NimBLE (MouldKingino) and PS4's Bluedroid Classic BT cannot coexist — NimBLE takes over BLE controller, PS4's Bluedroid init breaks NimBLE advertising silently
- **Fix**: Bypass NimBLE entirely, use Bluedroid BLE GAP API (`esp_ble_gap_*`) directly — both Classic BT (PS4) and BLE (MK advertising) share one Bluedroid stack
- **MK protocol**: MK60 BLE advertising with encrypted payloads (CRC16 + whitening), company ID 0xFFF0, 24-byte encrypted packets. Protocol from MouldKingino (MIT license)
- **PS4 MAC must be set before btStart()**: `esp_base_mac_addr_set()` must be called before any BT controller init. MK init calls `btStart()` first, so MAC must be set in early setup()
- **AsyncTCP socket exhaustion**: ESP32 AsyncTCP has ~4 concurrent connection slots. Multiple concurrent polls + slider sends + BLE radio contention → ack/rx timeouts → web UI freeze
- **Fix**: `Connection: close` on all JSON responses, reduced poll from 500ms→1000ms, eliminated separate MK status poll, MK BLE advertising only on value change + 500ms keepalive
- **Digital GPIO error**: `digitalRead()` on pins 32-35 without `pinMode()` spams errors. Fix: only read pins assigned to active digital-input channels
- **platformio.ini corruption**: Stray keystrokes in IDE can corrupt first line. First line was `ld it [env:esp32]` instead of `[env:esp32]`
- **Combined API endpoint**: Created `/api/state` returning outputs + raw-inputs + freq in one request (available but not yet wired to JS)

### Architecture changes
- `mk_input.h/.cpp` — MouldKing BLE motor controller with Bluedroid GAP advertising, curve playback, PS4/pot input routing
- MK channels are virtual outputs 16-21 in unified API (same `/api/output`, `/api/input`, `/api/curve-play` routes)
- `/api/outputs` returns 22 channels when MK connected, 16 when not
- Web UI creates 22 cards (MK cards hidden until connected), same full UI as PCA9685 cards
- `sendJsonClose()` helper adds `Connection: close` to all JSON responses

### Issues
- AsyncTCP ack/rx timeouts still occur under heavy load (many sequences + slider movement + MK + PS4)
- `/api/state` combined endpoint built but not yet wired to web UI JS (would eliminate multi-request polls)
- PS4 controller not yet confirmed working with MK (BT radio time contention possible)
- MK motor direction change responsiveness needs hardware verification

### To-do
- Wire `/api/state` to web UI JS (single request per poll cycle)
- Test PS4 + MK simultaneous operation
- Add upload_speed=115200 to platformio.ini
- Phase 1: Implement ELRS input
- Phase 2: Implement scripting engine
- Phase 3: Implement Blockly visual editor

### Ideas
- Use Server-Sent Events (SSE) instead of polling for real-time output value updates
- MK channels could support preset save/load alongside PCA9685 channels
- Consider me-no-dev/ESPAsyncWebServer replacement with lighter HTTP server if TCP issues persist

## Updated REST API Routes
| Method | Route | Description |
|--------|-------|-------------|
| GET | `/api/state` | Combined: outputs + raw-inputs + freq (one request) |
| GET | `/api/outputs` | All channel states (16 PCA + 6 MK when connected) |
| POST | `/api/output` | Set single channel (ch 0-15=PCA, 16-21=MK) |
| POST | `/api/input` | Set channel input source (ch 0-21) |
| POST | `/api/curve-play` | Start curve playback (ch 0-21) |
| POST | `/api/curve-stop` | Stop curve playback (ch 0-21) |
| POST | `/api/pot-assign` | Assign pot to channel (ch 0-21) |
| POST | `/api/mk-connect` | Connect/disconnect MK hub |
| GET | `/` | Web UI (http://pwm.local) |

## Updated Build Stats
- RAM: ~18.8%, Flash: ~62.2%
- No external BLE libraries needed (NimBLE/MouldKingino removed)

---

## Session — 2026-03-16 — TCP stability, gzip UI, preset MK support, PS4 init order

### Summary
Major session focused on TCP/WiFi stability under PS4+MK+WiFi load.
Gzip-compressed the web UI (60KB→13KB). Fixed PS4 init order. Added MK channels
to presets. Implemented TCP watchdog, compact poll endpoint, and fetch abort timeouts.

### Key findings
- **`Connection: close` makes TCP WORSE**: Closed connections enter TIME_WAIT for 120s, each holding a lwIP PCB slot. With polling every 1.5s, PCB pool exhausts in ~30s → req=0. Removed.
- **BLE GAP callback overhead**: Registering `esp_ble_gap_register_callback` starts BLE event processing that competes with lwIP on core 0 even when not advertising. Fix: removed callback, use synchronous advertising (stop → set data → 5ms delay → start).
- **BLE GAP deferred init**: Moving `esp_ble_gap_register_callback` from `init()` to first `connect()` call saves ~10KB heap when MK isn't used.
- **PS4 must init BEFORE MK**: PS4 library sets BT MAC via `esp_base_mac_addr_set` then calls `btStart()`. If MK calls `btStart()` first, MAC is already set with default. Fix: PS4 init first, MK piggybacks on existing Bluedroid.
- **`/api/state` compact poll**: Replaced heavy JSON (ArduinoJson + full channel config) with hand-built ~100 byte response `{"a":[angles...],"ps4":0,"mk":0}`. Eliminates 4-8KB heap churn per poll.
- **Gzip web UI**: 60KB → 13KB (78% reduction). `compress_ui.py` extracts HTML from `web_ui.h`, gzips, writes C byte array to `web_ui_gz.h`. Served with `Content-Encoding: gzip`. Must re-run after editing `web_ui.h`.
- **Fetch abort timeouts**: All browser fetch calls now use `AbortController` with 3-5s timeout. Prevents stuck TCP connections from blocking all future requests indefinitely.
- **TCP watchdog**: If `req=0` for 20 consecutive seconds (4 diag cycles), ESP auto-reboots to recover from permanently stuck TCP.
- **MK advertising oscillation**: PS4 stick jitter around center (±2 values) caused rapid advertise/stop toggling. Fix: ±2 deadzone snaps to center.
- **Preset load page reload**: After preset load, page reloads instead of trying to fetch state separately (competing for TCP connections). 13KB gzip page loads fast.
- **Dropbox .pio ignore**: `.dropboxignore` + NTFS alternate data stream `com.dropbox.ignored` prevents Dropbox from locking build artifacts.

### Architecture changes
- `compress_ui.py` — build tool: HTML → gzip → C byte array
- `web_ui_gz.h` — auto-generated gzip compressed web UI (do not edit directly)
- `/api/state` — compact poll endpoint (~100 bytes, no ArduinoJson)
- MK connect runs in FreeRTOS background task (non-blocking)
- MK BLE advertising: no GAP callback, synchronous with 5ms delay, 40ms interval
- Presets now include MK channels (`"mk"` key in JSON)
- TCP watchdog auto-reboots after 20s of req=0

### Issues
- AsyncTCP rx/ack timeouts still occur intermittently under heavy BT+WiFi load
- Website can become temporarily unreachable after MK connect (BLE init overhead)
- ESPAsyncWebServer may have inherent connection handling issues

### To-do
- Consider replacing ESPAsyncWebServer with lighter alternative
- Consider Server-Sent Events (SSE) to replace polling entirely
- ELRS RC input (Phase 1)
- Scripting engine (Phase 2)
- Blockly visual editor (Phase 3)

### Ideas
- External web app hosted on PC for advanced scripting/Blockly (uses REST API directly)
- Move to ESP32-S3 for more heap/flash and better WiFi
- WebSocket instead of REST for real-time bidirectional communication
