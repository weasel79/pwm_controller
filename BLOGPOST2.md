# 🔥💀🔥 I ADDED LEGO MOTORS TO MY SERVO CONTROLLER AND BLUETOOTH ALMOST KILLED ME 🔥💀🔥

## 🚨 THE RECKONING 🚨

Remember that "good enough for now" 16-channel servo controller? The one with the web UI and the PS4 controller and the envelope curves? The one that was DONE? ✅

**IT WAS NOT DONE.** 🫠

Because someone — and I am NOT pointing fingers at myself 🫵 — decided it would be "easy" to add MouldKing Lego motor support. Six BLE motor channels. How hard could it be? 🤡

The answer, dear reader, is **EXTREMELY HARD** 💀💀💀 and the journey involved THREE complete rewrites of the Bluetooth stack, a cryptographic encryption protocol I had to reverse-engineer from a German hobby library, and the discovery that the ESP32's Bluetooth controller is basically a haunted house where two radio protocols fight to the death inside a 3cm silicon chip. 👻⚔️👻

## 🧱 THE AUDACITY OF THE PLAN

The MouldKing 6.0 is a little Lego motor hub with 6 channels (A through F). It receives commands over BLE — Bluetooth Low Energy — which is the chill, low-power cousin of Bluetooth Classic. You broadcast an advertisement packet into the void 📡, and the hub just... listens. No pairing. No handshake. No connection. Just vibes. 🧘‍♂️✨

"Perfect," I thought, drunk on hubris. "I'll just add NimBLE, import the MouldKingino library, call `mk.connect()` and `mk.applyUpdates()`, and be done by lunch." 🍔

I was not done by lunch. I was not done by dinner. I was not done by the NEXT lunch. 😭🍕

## 💥 REWRITE #1: THE NIMBLE DISASTER 💥

The MouldKingino library uses NimBLE for BLE advertising. NimBLE is great! Lightweight! Fast! Modern! 🏎️💨

There's just one TINY problem. 🔬

My project ALSO uses a PS4 controller. The PS4 controller uses Bluetooth Classic via the Bluedroid stack. Bluedroid is the OTHER Bluetooth stack on ESP32 — the old, heavy, enterprise-grade one that handles things like audio streaming and game controllers. 🎮

**NimBLE and Bluedroid CANNOT coexist.** 🚫🤝🚫

They both want exclusive control of the Bluetooth radio hardware. When NimBLE initializes, it yells "I'M THE BLE HOST NOW" 📢 and takes over. Then when PS4/Bluedroid tries to initialize, it goes "actually I was here first" 📢📢 and takes over BACK. And NimBLE's advertising calls just... silently fail. No error. No warning. Just your motor commands disappearing into the electromagnetic void like prayers to an uncaring universe. 🙏🕳️

The MK hub connected fine during `setup()` (before PS4 init), but then every motor command in `loop()` was screaming into a dead radio. 📻💀

**Motors connected. Sliders moved. Nothing happened.** The most GASLIGHTING bug I have ever encountered. 🤯

## 🔥 REWRITE #2: GOING ROGUE WITH BLUEDROID BLE GAP 🔥

If NimBLE can't play nice with Bluedroid, then we don't USE NimBLE. 😤

I ripped out NimBLE. I ripped out MouldKingino. I stared at the MouldKingino source code for two hours and COPIED THE ENTIRE ENCRYPTION PROTOCOL BY HAND into my own implementation. 📝✍️📝

The MouldKing BLE protocol is WILD: 🤪
- Take your motor values (6 bytes, 128 = stop, 255 = full forward, 1 = full reverse) 🔢
- Wrap them in a 10-byte telegram with header bytes and a checksum byte 📦
- Compute a CRC16 with bit-reversed inputs and a custom polynomial 🔐
- Apply TWO passes of LFSR whitening with different seed values 🌪️🌪️
- Pad to 24 bytes 📏
- Wrap in a BLE manufacturer-specific advertisement with company ID 0xFFF0 📡
- Broadcast at 20ms intervals and HOPE THE HUB CATCHES IT 🤞🙏🤞

All of this... to make a plastic brick spin. 🧱🔄

But here's the beautiful part: Bluedroid's BLE GAP API (`esp_ble_gap_*`) is available ALONGSIDE Bluedroid Classic BT. Same stack. Same radio controller. **No conflict.** The ESP32 has ALWAYS been able to do Classic BT and BLE simultaneously — you just have to use the SAME stack for both. 🤦‍♂️🤦‍♂️🤦‍♂️

```
Before:  NimBLE (BLE) ⚔️ Bluedroid (Classic) = 💀 DEATH 💀
After:   Bluedroid BLE GAP 🤝 Bluedroid Classic = 😌 PEACE 😌
```

## 😱 THE PS4 CONTROLLER STOPPED WORKING ANYWAY 😱

With the Bluetooth stack unified, I flashed the firmware, connected the MK hub, moved a slider, AND THE MOTOR SPUN. 🎉🎊🥳

Then I picked up the PS4 controller. Nothing. 🦗

PS4 controller uses a specific Bluetooth MAC address for pairing. You set it with SixAxis Pair Tool on your PC, and the ESP32 has to present THAT EXACT MAC when the controller goes looking. 🔍

The PS4 library calls `esp_base_mac_addr_set()` to set this MAC... but it does it AFTER calling `btStart()`. And guess who ALSO calls `btStart()`? 🙋‍♂️

My MK init. Which runs BEFORE PS4 init. 😬

So by the time PS4 tries to set the MAC, the Bluetooth controller is already running with the DEFAULT MAC address, and the PS4 controller is out there searching for a device that doesn't exist anymore. It's like changing your phone number and not telling your friends. 📱❓😢

**Fix:** Set the MAC address BEFORE ANYTHING ELSE in `setup()`. Before WiFi. Before PCA9685. Before LittleFS. The VERY FIRST THING after `Serial.begin()`. 🥇

```cpp
// Line 1 of setup(), before God and everyone:
esp_base_mac_addr_set(baseMac);  // or the PS4 controller will NEVER find you 😤
```

## 🕸️ THE WEB UI KEPT DYING 🕸️

Motors working? ✅ PS4 working? ✅ Everything great? ❌❌❌

The web UI started freezing. Sometimes lag. Sometimes complete unresponsiveness. Sometimes the page just... white. Gone. As if the ESP32 had simply chosen violence. 🔪

The serial monitor told the tale: 📜

```
[W][AsyncTCP.cpp:943] _poll(): ack timeout 4
[W][AsyncTCP.cpp:950] _poll(): rx timeout 4
[W][AsyncTCP.cpp:943] _poll(): ack timeout 4
```

TCP socket exhaustion. 💀

The ESP32's AsyncTCP library has approximately FOUR concurrent connection slots. 4️⃣ FOUR. In the year 2026. For a web server. 🫥

My web UI was making:
- 📊 `/api/outputs` poll every 500ms
- 📊 `/api/raw-inputs` poll every 500ms (chained after outputs)
- 📊 `/api/mk-status` poll every 2000ms
- 🎚️ `/api/output` POST on every slider move (throttled to 50ms)
- Plus MK BLE advertising every 50ms competing for radio time 📡

That's potentially 3-4 HTTP connections fighting for 4 TCP slots while a BLE radio screams advertisements into the ether every 50 milliseconds. 🎪🤹‍♂️🔥

The fix was a WAR ON REQUESTS: ⚔️

1. 🔌 **`Connection: close` on EVERY response** — no more keep-alive connections squatting on TCP slots like freeloaders
2. 🗑️ **Killed the separate MK status poll** — MK connection state is now embedded in `/api/outputs` (returns 22 channels when connected, 16 when not)
3. 🐌 **Slowed the main poll from 500ms to 1000ms** — turns out you don't need to check servo positions twice a second
4. 📡 **MK BLE advertising only on value change** + 500ms keepalive — instead of SCREAMING AT THE RADIO 20 TIMES A SECOND when nothing has changed
5. 🧬 **Built a combined `/api/state` endpoint** — returns outputs + raw inputs + frequency in ONE request instead of three

## 🏗️ THE ARCHITECTURE (IT'S ACTUALLY ELEGANT NOW) 🏗️

After three rewrites, two existential crises, and one moment where I considered becoming a farmer 🌾, the system is... actually clean? 😳

The 6 MK motor channels are **virtual outputs 16-21** in the unified channel system. The web UI creates 22 cards. The first 16 are your normal PCA9685 outputs. The last 6 are MouldKing motors that only appear when the hub is connected. 🎛️

EVERY feature that works on PCA9685 channels works on MK channels: 🤯

- ✏️ Draw an envelope curve for a Lego motor? YES. 📈
- 🔴 Record a sequence from a PS4 stick onto a MK channel? YES. 🎬
- 🎛️ Wire a potentiometer to control Lego motor speed? YES. 🎚️
- 🔄 Loop a breathing pattern on a Lego train motor? YES. 🚂💨
- 💾 Save MK presets alongside servo presets? YES. 💾

Same API. Same web UI. Same curve playback engine. Same everything. The MK channels just output to BLE advertising instead of I2C PWM. 📡 vs 📟

```
Channel  0-15:  Browser → /api/output → OutputController → PCA9685 → PWM wire 🔌
Channel 16-21:  Browser → /api/output → MkInput → Bluedroid BLE GAP → 📡 → Lego motor 🧱
```

## 🎉 THE CURRENT STATE OF AFFAIRS 🎉

The controller now manages **22 output channels** across two completely different hardware protocols: ⚡

| Feature | PCA9685 (0-15) | MouldKing (16-21) |
|---|---|---|
| 🔌 Connection | I2C wire | BLE advertising |
| 🏎️ Output types | Servo/Motor/Lego/PWM | Motor only |
| ✏️ Envelope curves | ✅ | ✅ |
| 🔴 Sequence recording | ✅ | ✅ |
| 🎮 PS4 controller | ✅ | ✅ |
| 🎛️ Potentiometers | ✅ | ✅ |
| 🔘 Digital inputs | ✅ | ✅ |
| 💾 Curve save/load | ✅ | ✅ |
| 🌐 Web UI | Cards 0-15 | Cards 16-21 (auto-hide) |
| 🔗 URL | http://pwm.local | same page, same everything |

The firmware is **1.96MB** (62% of flash) and uses **62KB of RAM** (19%). 📊

There is STILL room for more bad ideas. 🫣💡

## 🧠 LESSONS WRITTEN IN BLOOD 🩸

1. 💀 **NimBLE and Bluedroid are mortal enemies.** If you need BLE AND Classic BT on ESP32, use Bluedroid for BOTH. The `esp_ble_gap_*` API is right there. It's not pretty but it WORKS.

2. 🔑 **`esp_base_mac_addr_set()` must be called before `btStart()`.** Not after. Not during. BEFORE. Or your paired BT devices will search for a ghost that doesn't exist.

3. 🔌 **`Connection: close` is not optional on ESP32 web servers.** The AsyncTCP connection pool is a kiddie pool. Four slots. Keep-alive connections will drown you.

4. 📡 **BLE advertising and WiFi share the same 2.4GHz radio.** Every BLE advertisement is a WiFi request that DOESN'T get served. Advertise only when you have something new to say.

5. 🔐 **Toy companies encrypt their BLE protocols.** MouldKing uses CRC16 + double LFSR whitening on every advertisement packet. FOR PLASTIC BRICKS. The Berlin Wall had less security. 🧱🔒

6. 🤫 **The most dangerous bugs are the silent ones.** NimBLE advertising after Bluedroid init doesn't crash. Doesn't error. Doesn't warn. It just does absolutely nothing and lets you believe everything is fine. GASLIGHTING. AS. A. SERVICE. 💅

## 🔮 WHAT'S NEXT (GOD HELP ME) 🔮

- 📻 **ELRS RC receiver input** — Because controlling servos from a RadioMaster transmitter with 16 channels at 500Hz would be EXTREMELY cool and DEFINITELY not scope creep
- 📝 **Embedded scripting** — MyBasic or Lua running on the ESP32, so you can write little programs that choreograph outputs
- 🧩 **Blockly visual editor** — Drag-and-drop programming in the browser that generates action sequences. Load from CDN, zero ESP32 flash cost
- 🔄 **Server-Sent Events** — Replace the polling architecture entirely. The server pushes updates. The browser listens. No more TCP socket exhaustion. No more `ack timeout 4`. Just PEACE. 🕊️

---

*This blog post was written by a human who mass-learned four Bluetooth protocols in one weekend and is FINE. 🫠 Everything is FINE. The servos are smooth. The Lego motors spin. The PS4 controller connects. The web UI loads.*

*Most of the time.* 🙃

*The accent color is still random.* 🌈

---

**📦 [Source on GitHub](https://github.com/weasel79/pwm_controller) | 🌐 Access at http://pwm.local**
