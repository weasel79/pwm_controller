# ESP32 Servo Controller with PCA9685 — Project Specification

> This spec fully describes the project. An AI or developer should be able to
> regenerate the entire codebase from this document alone.

---

## 1. Overview

A multi-interface servo controller running on an ESP32 (original, dual-core Xtensa).
Uses a PCA9685 16-channel PWM driver over I2C to control standard hobby servos.
Provides four independent control interfaces and sequence record/replay functionality.

## 2. Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32 (ESP-WROOM-32), e.g. `esp32dev` board |
| PWM driver | PCA9685, I2C address `0x40` |
| I2C bus | SDA = GPIO 21, SCL = GPIO 22 |
| Servos | 5–16 standard hobby servos (SG90, MG996R, etc.) |
| Analog inputs | ADC1 only (GPIOs 32–39) — ADC2 conflicts with WiFi |
| Default pot pins | GPIO 34, 35, 32, 33 |
| Default joystick pins | X = GPIO 36, Y = GPIO 39 |

## 3. Toolchain

| Item | Detail |
|------|--------|
| Framework | Arduino via PlatformIO |
| Platform | `espressif32` |
| Board | `esp32dev` |
| Partition scheme | `huge_app.csv` (3 MB app, no OTA) — needed for BLE + WiFi |
| Filesystem | LittleFS |
| Build command | `pio run` |
| Upload command | `pio run -t upload` |
| Serial monitor | `pio device monitor` (115200 baud) |

### Library Dependencies

| Library | Purpose |
|---------|---------|
| `adafruit/Adafruit PWM Servo Driver Library@^3.0.2` | PCA9685 I2C driver |
| `me-no-dev/ESPAsyncWebServer@^1.2.4` | Async HTTP server |
| `me-no-dev/AsyncTCP@^1.1.1` | TCP backend for async web server |
| `bblanchon/ArduinoJson@^7.3.0` | JSON serialization/deserialization |
| Built-in: `Wire`, `WiFi`, `BLE`, `LittleFS` | ESP32 Arduino core |

## 4. Project Structure

```
servo-geloet-esp32/
  platformio.ini
  SPEC.md                         # This file
  src/
    main.cpp                      # setup() / loop(), wires all modules
    config.h                      # All pin assignments, limits, credentials, constants
    servo_controller.h / .cpp     # PCA9685 abstraction
    wifi_controller.h / .cpp      # WiFi + web server + REST API
    web_ui.h                      # Embedded HTML/JS (PROGMEM raw string literal)
    ble_controller.h / .cpp       # BLE server + characteristics
    analog_input.h / .cpp         # Potentiometer + joystick reading
    sequence_recorder.h / .cpp    # Record / save / load / play sequences
  data/                           # (reserved) LittleFS data partition
```

## 5. Module Specifications

### 5.1 `config.h` — Central Configuration

All compile-time constants in one header. No `.cpp` file.

- WiFi: SSID, password, AP fallback SSID/password, STA/AP mode toggle
- I2C: SDA pin, SCL pin, PCA9685 address
- Servo: count (16), frequency (50 Hz), default min/max pulse (500–2500 us), default angle (90)
- Analog: pin assignments, ADC resolution (12-bit), deadzone (30), smoothing alpha (0.15), update interval (20 ms)
- Sequence: directory path (`/sequences`), frame interval (50 ms / 20 fps), max frames (6000)
- BLE: device name, service UUID, characteristic UUIDs (servo, bulk, state)
- Web server: port (80)

### 5.2 `servo_controller` — PCA9685 Abstraction

**Struct `ServoChannel`:**
- `channel` (uint8), `minUs` / `maxUs` (uint16), `currentAngle` / `targetAngle` (uint8), `name` (char[16]), `enabled` (bool)

**Class `ServoController`:**
- `init()` — Wire.begin, PCA9685 begin, set oscillator to 25 MHz, set PWM freq to 50 Hz, initialize all 16 channels to defaults, move all to default angle
- `setAngle(channel, angle)` — constrain 0–180, set target; if no smoothing, write immediately
- `getAngle(channel)` — return cached current angle
- `setAllAngles(angles[], count)` — bulk set
- `getAllAngles(outAngles[], count)` — bulk read
- `getChannel(ch)` — return const ref to ServoChannel struct
- `setChannelRange(channel, minUs, maxUs)` — per-channel pulse calibration
- `setChannelName(channel, name)` — set display name
- `setSmoothingSteps(steps)` — 0 = instant, N = interpolate over N loop iterations
- `update()` — called in loop(); if smoothing enabled, step currentAngle toward targetAngle

**PWM calculation:** `ticks = microseconds * 4096 / 20000` (50 Hz = 20 ms period)

### 5.3 `wifi_controller` + `web_ui` — WiFi, Web UI, REST API

**WiFi setup:**
- If `WIFI_STA_MODE` is true: connect to configured SSID with 10 s timeout; on failure, fall back to AP mode
- If false (default): start AP directly with SSID `ServoController`, password `servo1234`
- Print IP to Serial

**Web UI (`web_ui.h`):**
- Single HTML page stored as `PROGMEM` raw string literal
- Dark theme, responsive CSS grid layout
- 16 slider cards (0–180 range), each showing servo name + current angle
- Sliders send `fetch()` POST to REST API on input (throttled ~50 ms)
- Buttons: Record, Stop, Play Last, Save (prompts for name)
- Saved sequences section: list with Play / Delete buttons per entry
- State loaded on init via `GET /api/servos`

**REST API (AsyncWebServer, port 80):**

| Method | Endpoint | Body | Response |
|--------|----------|------|----------|
| GET | `/` | — | HTML web UI |
| GET | `/api/servos` | — | `[{"channel":0,"angle":90,"name":"Servo 0"}, ...]` |
| POST | `/api/servo` | `{"channel":N,"angle":N}` | `{"ok":true}` |
| POST | `/api/servos` | `{"angles":[N,N,...]}` | `{"ok":true}` |
| POST | `/api/sequence/record` | — | `{"ok":true}` |
| POST | `/api/sequence/stop` | — | `{"ok":true}` |
| POST | `/api/sequence/play` | `{"name":"X","loop":bool}` | `{"ok":true}` or 404 |
| POST | `/api/sequence/save` | `{"name":"X"}` | `{"ok":true}` or 500 |
| POST | `/api/sequence/delete` | `{"name":"X"}` | `{"ok":true}` or 404 |
| GET | `/api/sequences` | — | `["seq1","seq2",...]` |

POST bodies with JSON use the `onBody` handler pattern of ESPAsyncWebServer (3-arg `on()` with body callback).

### 5.4 `ble_controller` — Bluetooth Low Energy

**BLE Server** with one Service (custom UUID).

**Characteristics:**

| Name | UUID suffix | Properties | Format |
|------|-------------|------------|--------|
| Servo control | `...26a8` | Write | Text: `"channel:angle"` e.g. `"5:90"` |
| Bulk update | `...26a9` | Write | Binary: 16 bytes, one per channel (angle 0–180) |
| State notify | `...26aa` | Read + Notify | Binary: 16 bytes (current angles) |

- On client connect: log to Serial
- On client disconnect: restart advertising
- State notification sent periodically from `loop()` (every 200 ms) when a client is connected
- Uses static globals for callbacks (ESP32 BLE API requires it)

### 5.5 `analog_input` — Potentiometers + Joysticks

**Struct `PotMapping`:** adcPin, servoChannel, smoothedValue (float), lastRawValue, active flag

**Struct `JoystickMapping`:** xPin, yPin, servoChannelX/Y, smoothedX/Y, lastRawX/Y

**Class `AnalogInput`:**
- `init(servoCtrl)` — store pointer, set ADC resolution
- `addPot(adcPin, servoChannel)` — register pot mapping (max 8)
- `addJoystick(xPin, yPin, chX, chY)` — register joystick mapping (max 4)
- `update()` — called in loop(); rate-limited to `ANALOG_UPDATE_MS`
  - Read each pot/joystick ADC
  - Apply exponential moving average: `smoothed += alpha * (raw - smoothed)`
  - If change exceeds deadzone threshold, map to 0–180 and call `servoCtrl->setAngle()`
  - Track active state per pot
- `isChannelActive(ch)` — returns true if a pot controlling this channel is currently changing

**ADC to angle mapping:** `map(value, 0, 4095, 0, 180)`

**Important:** Only ADC1 pins (GPIO 32–39) work when WiFi is enabled.

### 5.6 `sequence_recorder` — Record / Save / Load / Play

**Struct `SequenceFrame`:** timestampMs (uint32), angles[16] (uint8)

**Class `SequenceRecorder`:**
- `init(servoCtrl)` — allocate frame buffer (`new SequenceFrame[SEQUENCE_MAX_FRAMES]`)
- `update()` — called in loop():
  - If recording: capture frame every `SEQUENCE_FRAME_MS`
  - If playing: advance playback based on elapsed time, apply frame angles via `setAllAngles()`; handle looping or stop at end
- `startRecording()` — stop playback, reset frame count, record start time
- `stopRecording()` — set recording flag to false
- `loadSequence(name)` — read JSON from LittleFS `/sequences/<name>.json`, parse into frame buffer
- `startPlayback(loop)` — start from frame 0
- `stopPlayback()` — halt playback
- `saveSequence(name)` — write frame buffer as JSON to LittleFS (streaming write to save memory)
- `deleteSequence(name)` — remove file from LittleFS
- `listSequencesJson()` — iterate `/sequences/` directory, return JSON array of names (strip `.json` extension)

**File format** (`/sequences/<name>.json`):
```json
{"frames":[{"t":0,"a":[90,90,90,...]},{"t":50,"a":[91,89,90,...]}, ...]}
```

### 5.7 `main.cpp` — Integration

```
setup():
  Serial.begin(115200)
  LittleFS.begin(true)           // format on first use
  servoController.init()
  sequenceRecorder.init(&servoController)
  wifiController.init(&servoController, &sequenceRecorder)
  bleController.init(&servoController)
  analogInput.init(&servoController)
  // analogInput.addPot(...) — uncomment per wiring
  // analogInput.addJoystick(...) — uncomment per wiring

loop():
  analogInput.update()           // Read pots/joysticks
  sequenceRecorder.update()      // Advance recording/playback
  servoController.update()       // Smooth servo movement
  bleController.updateStateNotification()  // Every 200ms if connected
  // WiFi + BLE callbacks are async — no polling needed
```

**BLE state notification** is rate-limited to every 200 ms via a `millis()` check in `loop()`.

## 6. Input Priority

When multiple sources control the same channel:
- Analog input (pots/joysticks) takes priority when actively changing
- WiFi/BLE commands apply immediately but can be overridden by analog
- Sequence playback locks channels until stopped

## 7. Build Configuration (`platformio.ini`)

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit PWM Servo Driver Library@^3.0.2
    me-no-dev/ESPAsyncWebServer@^1.2.4
    me-no-dev/AsyncTCP@^1.1.1
    bblanchon/ArduinoJson@^7.3.0
board_build.partitions = huge_app.csv
board_build.filesystem = littlefs
build_flags =
    -DCORE_DEBUG_LEVEL=3
```

## 8. Verification Checklist

1. `pio run` — compiles with zero errors (expect ~54% flash, ~18% RAM)
2. `pio run -t upload` — flash to ESP32 via USB
3. Serial monitor shows WiFi IP + BLE advertising message
4. Browse to ESP32 IP — web UI with 16 sliders
5. Moving a slider changes servo position via REST API
6. `GET /api/servos` returns JSON array of all positions
7. BLE: use nRF Connect to write `"0:45"` to servo characteristic
8. Pot on GPIO 34 (if wired) tracks to mapped servo channel
9. Record → move servos → Stop → Save → power cycle → Load → Play reproduces movement
