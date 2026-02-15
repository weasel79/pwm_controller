# 🔧 I Built a 16-Channel Servo Controller and It Got Out of Hand

It started with a PCA9685 breakout board, an ESP32, and three servos that needed to wiggle. 🤷
It ended with a PS4 controller, envelope curves, sequence recording, a preset system,
and a web UI that picks a random accent color every time you open it. 🎨

Here's how a weekend solder project turned into a full animation workstation. 🚀

## 🔌 The Setup

The hardware is refreshingly simple. An ESP32 talks to a PCA9685 16-channel PWM driver
over I2C. That's it. Two wires (plus power). The PCA9685 handles the actual PWM generation
at 50Hz for servos or up to 1kHz for DC motors, freeing the ESP32 to do more interesting things.

The interesting things turned out to be *very* interesting. 👀

## 🎚️ "I Just Need a Web Slider"

The first version was exactly what you'd expect. ESP32 starts a WiFi access point, serves a
web page with 16 range sliders, each one sends a POST request when you drag it. Servo moves.
Done. Weekend project complete. ✅

But then I wanted to make a servo sweep back and forth automatically. And then I wanted to
control the sweep shape. And then I wanted to record movements and play them back.
And *then* I paired a PS4 controller. 🎮

You know how it goes. 😅

## ⚙️ Five Flavors of Output

Not everything connected to a PCA9685 is a servo. The controller now supports five output types:

- 🦾 **Servo** -- Standard hobby servos at 50Hz. The 0-180 range maps to configurable pulse widths (default 500-2500us).
- 🏎️ **DC Motor** -- Full duty cycle PWM. 0 = stop, 180 = full speed.
- 🧱 **Lego Motor** -- Same as DC motor but it felt important to distinguish them.
- 💡 **Raw PWM** -- For LEDs, heaters, fans, whatever takes a duty cycle.
- 🔇 **Motor 1kHz** -- DC motors that sound less whiny at higher PWM frequencies. When no servos are active, the PCA9685 switches to 1kHz automatically.

Each of the 16 channels can be a different type. Mix servos and motors on the same board. 🎛️

## 🕹️ Six Ways to Move a Servo

Every channel has an input source selector. This is where it gets fun:

🎚️ **Manual** -- The classic slider. Drag it, servo moves.

✏️ **Envelope** -- A mini curve editor right in the card. Click on a canvas to add control points.
Drag them around. Double-click to delete. Set a duration, toggle loop mode, hit play.
The ESP32 interpolates between your points at ~1kHz for motion so smooth it looks like
the servo is on rails. 🧈

🔴 **Sequence** -- Hit record, move a servo (from any input source), hit stop. You just captured a
motion sequence at 50ms resolution. Play it back at 0.25x to 4x speed. Save it with a name.
Load it later. The recording can come from the slider, a potentiometer, or a PS4 controller --
just pick the source from a dropdown before you hit record. 🎬

🎛️ **Potentiometer** -- Wire a pot to one of four ADC pins and map it to any channel.
The analog input runs through exponential smoothing and a deadzone filter so your servos
don't jitter. Reassign which pot drives which channel from the web UI.

🎮 **PS4 Controller** -- Pair a DualShock 4 and suddenly you have two analog sticks (4 axes),
two analog triggers, two bumpers, and four face buttons all mappable to output channels.
The sticks give you 0-180 range with a center deadzone. The buttons are toggles.
Walking around controlling servos with a game controller is unreasonably satisfying. 😎

## 🧈 The Smoothness Problem

Early envelope playback was steppy. The browser was sending angle values over WiFi at 20Hz
(once every 50ms), and each value was an integer 0-180. That's 20 discrete steps per second,
each quantized to one of 181 values. You could *hear* the steps in the servo. 😬

The fix was to move all the math to the ESP32. Now the browser sends the complete curve -- all
the control points, the duration, and whether to loop -- in a single POST request. The ESP32
stores the points in RAM and does linear interpolation every loop iteration (~1kHz). Everything
uses float precision: the interpolated angle, the pulse width calculation, even the PCA9685
tick count. The result is sub-tick resolution from a 12-bit PWM chip. 🤯

```
🌐 Browser: "Here are 5 points over 3 seconds, loop=true"
    |
    | 📨 one POST request
    v
🧠 ESP32: "Got it. I'll interpolate 3000 times per second forever."
    |
    | ⚡ every ~1ms
    v
⚙️ PCA9685: smooth, continuous PWM signal
```

The browser still runs an animation loop, but only for the visual cursor on the canvas.
No more network latency in the servo path. 🏎️💨

## 💾 Presets: Remember Everything

After spending 10 minutes setting up 8 channels with different types, input sources, pot
assignments, and values, losing it all to a power cycle felt criminal. 😤 The preset system
saves the entire board state -- all 16 channels with their type, input source, current value,
name, min/max pulse range, and pot assignment -- as a JSON file on the ESP32's LittleFS
filesystem.

Envelopes and sequences have their own save/load system too. Draw a breathing pattern, save it
as "breathe" 🌬️. Record a head-turn sequence from a PS4 stick, save it as "look-around" 👀. Mix and
match them across channels.

## 🎨 The UI

The web UI is a single HTML file embedded in the firmware as a PROGMEM string literal. No
build step, no bundler, no node_modules. Just 1100 lines of HTML, CSS, and vanilla JavaScript
squeezed into a C++ raw string. 📦

It's dark-themed 🌙 (of course), responsive 📱 (works on phones), and picks a random HSL accent color
on every page load because I couldn't commit to a color. 🌈

Each output gets a card with:
- 🔌 Type selector (servo/motor/lego/pwm/motor1k)
- 🔀 Input source selector
- 🎯 Input-specific panel that changes based on what you pick:
  - 🎚️ Manual: slider
  - ✏️ Envelope: canvas editor + play/stop/duration/loop + save/load
  - 🔴 Sequence: canvas display + rec/play/stop/loop/speed + save/load + input source selector for recording
  - 🎛️ Pot: dropdown to pick which physical pot drives this channel
  - 🎮 PS4: grayed-out slider showing the current value (read-only, the controller is in charge)

Below the cards: a preset section (save/load/delete) and an OTA firmware update section
(drag a .bin file, upload, the ESP reboots with new firmware). 📡

## 🧠 Things I Learned

🐛 **ESPAsyncWebServer has a prefix-matching bug.** If you have routes `/api/preset` and
`/api/preset-save`, the first one catches both. Use hyphens and distinct prefixes.

📂 **ESP32 LittleFS `file.name()` returns the full path.** Not just the filename.
`/presets/mypreset.json`, not `mypreset.json`. Spent an embarrassing amount of time on that one. 🤦

⚡ **ADC2 doesn't work when WiFi is on.** ESP32 uses ADC2 for WiFi internally. Stick to
ADC1 pins (GPIO 32-39) for potentiometers.

🎯 **Float precision matters more than update rate.** Going from `uint8_t angle` to `float value`
throughout the PWM chain did more for smoothness than any amount of update rate tuning.
Integer quantization to 181 discrete values was the real enemy.

🤝 **PS4 controllers over Bluetooth Classic coexist fine with WiFi.** Just call
`esp_coex_preference_set(ESP_COEX_PREFER_WIFI)` and both work simultaneously. The PS4 library
handles all the Bluetooth complexity.

## 🧰 The Stack

- 🧠 ESP32-WROOM-32 (the original, not S2/S3/C3)
- ⚙️ PCA9685 16-channel PWM driver
- 🔨 PlatformIO + Arduino framework
- 🌐 ESPAsyncWebServer for the web UI and REST API
- 📋 ArduinoJson v7 for serialization
- 🎮 aed3/PS4-esp32 for PS4 controller support
- 💾 LittleFS for persistent storage
- 🍦 Vanilla JS because React on an ESP32 would be unhinged

## 🏁 Is It Done?

No. It's never done. 😂 But it controls 16 things from a web page, a game controller, four
potentiometers, drawn curves, and recorded sequences. The servos are smooth. The presets
persist. The accent color is always different. 🌈

That's good enough for now. ✨

---

*The firmware is ~1.7MB (54% of flash) and uses ~58KB of RAM (18%). There's room for more
bad ideas.* 🫣
