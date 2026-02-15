# 🔧 servo-geloet-esp32

A 16-channel output controller for servos, motors, and LEDs running on an ESP32 + PCA9685.
Ships with a sleek dark-themed web UI, PS4 controller support, potentiometer inputs,
envelope curves, sequence recording, and a preset system -- all in one firmware. ✨

```
  🌐 Browser               🧠 ESP32
  +----------------+    +--------------------+
  | Dark Web UI    | -- | WiFi (AP or STA)   |
  | 16 output cards|    | PCA9685 over I2C   |--- 16 PWM outputs ⚙️
  | Envelope editor|    | Curve interpolation|
  | Sequence rec.  |    | LittleFS presets   |
  +----------------+    | PS4 Bluetooth      |--- 🎮 DualShock 4
                        | 4x ADC pots        |--- 🎛️ Potentiometers
                        +--------------------+
```

## 🤔 What it does

Plug in up to 16 servos, DC motors, Lego motors, or raw PWM devices into a PCA9685 breakout.
Open the web UI on your phone 📱. Move sliders. Draw envelope curves. Record sequences.
Plug in a PS4 controller and map sticks, triggers, and buttons to any output.
Save everything as presets. 💾

## ⭐ Features

🔌 **5 output types** -- Servo (50Hz PWM), DC Motor, Lego Motor, Raw PWM, DC Motor @ 1kHz

🎚️ **6 input sources per channel** -- Manual slider, Envelope curve, Sequence playback, Potentiometer, PS4 controller axes/buttons

✏️ **Envelope editor** -- Draw curves with control points on a canvas. Click to add points, drag to reshape, double-click to delete. Set duration (0.5-30s) and loop mode. Curves play on the ESP32 at ~1kHz for buttery-smooth servo motion. 🧈

🔴 **Sequence recorder** -- Record live movements from any input source (slider, pots, PS4 controller) at 50ms resolution. Play back with adjustable speed (0.25x - 4x). Save/load named sequences.

🎮 **PS4 controller** -- Pair a DualShock 4 via Bluetooth Classic. Map left/right sticks (X/Y), L2/R2 triggers, L1/R1, and face buttons (Cross, Circle, Square, Triangle) to any output channel.

💾 **Preset system** -- Save and load the entire board configuration (all 16 channels: types, input sources, values, names, ranges, pot assignments) with one click.

📡 **OTA updates** -- Upload new firmware from the web UI. No USB cable needed after first flash.

🎨 **Random accent color** -- Every page load picks a new hue. Because life is too short for static UIs.

## 🛠️ Hardware

| Component | Detail |
|-----------|--------|
| 🧠 MCU | ESP32-WROOM-32 (`esp32dev`) |
| ⚡ PWM driver | PCA9685 on I2C (SDA=21, SCL=22, addr 0x40) |
| ⚙️ Outputs | Up to 16 servos/motors/LEDs |
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
5. 🌐 Connect to the ESP32's IP (check serial monitor) or the `OutputController` WiFi network (password: `servo1234`)
6. 🎉 Open the web UI and start moving things!

## 🏗️ Building

Requires [PlatformIO](https://platformio.org/).

```bash
pio run              # 🔨 compile
pio run -t upload    # ⚡ flash via USB
pio device monitor   # 📺 serial output (115200 baud)
```

## 📁 Project structure

```
src/
  config.h              📋 Pin assignments, WiFi creds, constants
  main.cpp              🏠 setup() / loop(), wires all modules
  output_controller.h   ⚙️ PCA9685 abstraction, float PWM, curve playback
  output_controller.cpp
  wifi_controller.h     🌐 WiFi + AsyncWebServer + REST API
  wifi_controller.cpp
  web_ui.h              🎨 Embedded HTML/CSS/JS (PROGMEM raw literal)
  ps4_input.h           🎮 PS4 DualShock 4 via Bluetooth Classic
  ps4_input.cpp
  analog_input.h        🎛️ Potentiometer reading with smoothing + deadzone
  analog_input.cpp
```

## 📡 REST API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/outputs` | 📊 All channel states (angle, type, input, name, pot) |
| POST | `/api/output` | 🎚️ Set single channel: `{channel, angle}` |
| POST | `/api/outputs` | 🎚️ Set multiple: `{angles: [...]}` |
| POST | `/api/output/type` | 🔌 Set type: `{channel, type}` |
| POST | `/api/input` | 🔀 Set input source: `{channel, input}` |
| POST | `/api/pot-assign` | 🎛️ Map pot to channel: `{channel, pot}` |
| POST | `/api/curve-play` | ▶️ Send curve for ESP-side playback: `{channel, duration, loop, points}` |
| POST | `/api/curve-stop` | ⏹️ Stop curve: `{channel}` |
| POST | `/api/preset-save` | 💾 Save preset: `{name}` |
| POST | `/api/preset-load` | 📂 Load preset: `{name}` |
| GET | `/api/presets` | 📋 List saved presets |
| POST | `/api/seq-save` | 💾 Save sequence: `{channel, name, points}` |
| POST | `/api/seq-load` | 📂 Load sequence: `{name}` |
| GET | `/api/seq-list` | 📋 List saved sequences |
| POST | `/api/env-save` | 💾 Save envelope: `{name, duration, loop, points}` |
| POST | `/api/env-load` | 📂 Load envelope: `{name}` |
| GET | `/api/env-list` | 📋 List saved envelopes |
| POST | `/api/ota` | 📡 Upload firmware binary |

## 🧈 How curve playback works

Old approach: browser sends angle values over WiFi 20 times per second. Jittery, quantized, laggy. 😬

New approach: browser sends **all curve points once**. The ESP32 stores them in RAM and interpolates between points at loop speed (~1kHz). Float precision throughout the PWM chain gives sub-tick PCA9685 resolution. The result is servo motion so smooth you'll think it's analog. 😍

## 📄 License

Do whatever you want with it. Solder responsibly. 🫡
