# Universal PWM Control (servo-geloet-esp32)

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
