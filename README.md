# 🔧 pwm_controller

A **22-channel** output controller for servos, motors, LEDs, and MouldKing Lego motors running on an ESP32 + PCA9685 + Bluedroid BLE.
Ships with a sleek dark-themed web UI at **http://pwm.local**, PS4 controller support, potentiometer inputs,
envelope curves, sequence recording, and a preset system -- all in one firmware. ✨

```
  🌐 Browser               🧠 ESP32
  +----------------+    +-----------------------------+
  | Dark Web UI    | -- | WiFi STA (mDNS: pwm.local) |
  | 22 output cards|    | PCA9685 over I2C            |--- 16 PWM outputs ⚙️
  | Envelope editor|    | Bluedroid BLE GAP           |--- 6 MouldKing motors 🧱
  | Sequence rec.  |    | Curve interpolation @ 1kHz  |
  | Presets & OTA  |    | LittleFS presets            |
  +----------------+    | PS4 Bluetooth Classic       |--- 🎮 DualShock 4
                        | 4x ADC pots                 |--- 🎛️ Potentiometers
                        +-----------------------------+
```

## 🤔 What it does

Plug in up to **16 servos/motors/LEDs** into a PCA9685 breakout AND control **6 MouldKing Lego motors** over BLE — all from the same web UI. Open http://pwm.local on your phone 📱. Move sliders. Draw envelope curves. Record sequences.
Plug in a PS4 controller and map sticks, triggers, and buttons to any of the 22 outputs.
Save everything as presets. 💾

## ⭐ Features

🔌 **5 output types** -- Servo (50Hz PWM), DC Motor, Lego Motor, Raw PWM, DC Motor @ 1kHz

🧱 **MouldKing BLE motors** -- 6 additional motor channels (MK A-F) via Bluedroid BLE advertising. Connect/disconnect from the web UI. Full bidirectional speed control (-100% to +100%).

🎚️ **7 input sources per channel** -- Manual slider, Envelope curve, Sequence playback, Potentiometer, PS4 controller, Digital I/O — available on ALL 22 channels (PCA9685 and MK alike)

✏️ **Envelope editor** -- Draw curves with control points on a canvas. Click to add points, drag to reshape, double-click to delete. Set duration (0.5-30s) and loop mode. Curves play on the ESP32 at ~1kHz for buttery-smooth motion. 🧈

🔴 **Sequence recorder** -- Record live movements from any input source (slider, pots, PS4 controller) at 50ms resolution. Play back with adjustable speed (0.25x - 4x). Save/load named sequences.

🎮 **PS4 controller** -- Pair a DualShock 4 via Bluetooth Classic. Map left/right sticks (X/Y), L2/R2 triggers, L1/R1, and face buttons to any output channel — including MK motors.

💾 **Preset system** -- Save and load the entire board configuration with one click.

📡 **OTA updates** -- Upload new firmware from the web UI. No USB cable needed after first flash.

🌐 **mDNS** -- Access at http://pwm.local instead of hunting for IP addresses.

🎨 **Random accent color** -- Every page load picks a new hue. Because life is too short for static UIs. 🌈

## 🛠️ Hardware

| Component | Detail |
|-----------|--------|
| 🧠 MCU | ESP32-WROOM-32 (`esp32dev`) |
| ⚡ PWM driver | PCA9685 on I2C (SDA=21, SCL=22, addr 0x40) |
| ⚙️ PWM Outputs | Up to 16 servos/motors/LEDs |
| 🧱 BLE Motors | MouldKing 6.0 hub (6 channels, connect via web UI) |
| 🎛️ Pots | 4x on GPIO 34, 35, 32, 33 (ADC1 only) |
| 🎮 PS4 controller | DualShock 4 via Bluetooth Classic |

## 🚀 Quick start

1. 🔗 Wire a PCA9685 to your ESP32 (SDA=21, SCL=22, VCC=3.3V)
2. 🔌 Connect servos to PCA9685 channels 0-15
3. 📝 Edit `src/config.h` -- set your WiFi credentials or use AP mode
4. ⚡ Flash:
   ```
   pio run -t upload
   ```
5. 🌐 Open **http://pwm.local** (or check serial monitor for IP)
6. 🧱 Optional: Power on a MouldKing 6.0 hub during the 3-second connect window at boot
7. 🎉 Start moving things!

## 🏗️ Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run              # 🔨 compile
pio run -t upload    # ⚡ flash via USB
pio device monitor   # 📺 serial output (115200 baud)
```

**Build stats:** RAM ~18.8%, Flash ~62.2% 📊

## 📁 Project structure

```
src/
  config.h              📋 Pin assignments, WiFi creds, MK/timing constants
  main.cpp              🏠 setup() / loop(), wires all modules, sets BT MAC early
  output_controller.h   ⚙️ PCA9685 abstraction, float PWM, curve playback (ch 0-15)
  output_controller.cpp
  mk_input.h            🧱 MouldKing BLE motor controller (ch 16-21)
  mk_input.cpp             Bluedroid BLE GAP advertising, MK60 protocol + crypto
  wifi_controller.h     🌐 WiFi + mDNS + AsyncWebServer + REST API
  wifi_controller.cpp      Routes ch 0-15 to PCA9685, ch 16-21 to MK
  web_ui.h              🎨 Embedded HTML/CSS/JS (PROGMEM raw literal, 22 cards)
  ps4_input.h           🎮 PS4 DualShock 4 via Bluetooth Classic
  ps4_input.cpp
  analog_input.h        🎛️ Potentiometer reading with smoothing + deadzone
  analog_input.cpp
  digital_input.h       🔘 GPIO digital inputs with debounce/toggle/latch
  digital_input.cpp
  patch_ps4lib.py       🔧 Pre-build: patches PS4Controller delay(250) crash
```

## 📡 REST API

All output routes accept channel 0-15 (PCA9685) or 16-21 (MouldKing) transparently.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/state` | 🔄 Combined: all outputs + raw inputs + freq (single request) |
| GET | `/api/outputs` | 📊 All channel states (16 PCA + 6 MK when connected) |
| POST | `/api/output` | 🎚️ Set single channel: `{channel, angle}` |
| POST | `/api/input` | 🔀 Set input source: `{channel, input}` |
| POST | `/api/curve-play` | ▶️ Send curve: `{channel, duration, loop, points}` |
| POST | `/api/curve-stop` | ⏹️ Stop curve: `{channel}` |
| POST | `/api/pot-assign` | 🎛️ Map pot: `{channel, pot}` |
| POST | `/api/mk-connect` | 🧱 Connect/disconnect MK: `{action}` |
| POST | `/api/preset-save` | 💾 Save preset: `{name}` |
| POST | `/api/preset-load` | 📂 Load preset: `{name}` |
| GET | `/api/presets` | 📋 List saved presets |
| POST | `/api/seq-save` | 💾 Save sequence |
| POST | `/api/seq-load` | 📂 Load sequence |
| GET | `/api/seq-list` | 📋 List sequences |
| POST | `/api/env-save` | 💾 Save envelope |
| POST | `/api/env-load` | 📂 Load envelope |
| GET | `/api/env-list` | 📋 List envelopes |
| POST | `/api/ota` | 📡 Upload firmware binary |

## 🧈 How curve playback works

Browser sends **all curve points once** → ESP32 stores in RAM → interpolates at ~1kHz with float precision → sub-tick PCA9685 resolution. Same engine drives both PCA9685 servo curves and MouldKing motor sequences. 😍

## 🔧 Technical notes

- **BLE + Classic BT coexistence:** Both use Bluedroid stack. MK uses `esp_ble_gap_*` for BLE advertising, PS4 uses Bluedroid Classic. No NimBLE dependency.
- **PS4 MAC address:** Set via `esp_base_mac_addr_set()` before `btStart()` in early setup. Pair with SixAxis Pair Tool.
- **TCP optimization:** All JSON responses use `Connection: close` to prevent socket exhaustion on ESP32's limited AsyncTCP pool.
- **MK protocol:** MK60 encrypted BLE advertising (CRC16 + LFSR whitening), inlined from MouldKingino (MIT license).

## 📝 Blog posts

- [BLOGPOST.md](BLOGPOST.md) -- How a weekend solder project became a 16-channel animation workstation
- [BLOGPOST2.md](BLOGPOST2.md) -- Adding Lego motors and the Bluetooth stack nearly killed me

## 📄 License

Do whatever you want with it. Solder responsibly. 🫡
