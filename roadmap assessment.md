# Universal PWM Control — Roadmap Assessment

Assessment of three proposed features: ELRS RC receiver input, embedded scripting language, and block-based visual programming UI.

**Current system**: ESP32 + PCA9685 16-ch PWM, WiFi (ESPAsyncWebServer), PS4 BT Classic, analog/digital inputs, envelope/sequence playback, presets. Free heap: ~54KB. Flash: ~54.5% of huge_app partition.

---

## 1. ELRS RC Receiver Input

### Verdict: HIGHLY FEASIBLE — Low risk, high value

### What it is
ExpressLRS (ELRS) receivers output CRSF serial protocol over UART. The RadioMaster Pocket transmitter provides up to 16 channels: 4 gimbal axes, 5 switches (SA-SE), 1 pot (S1), and 6 user-assignable.

### Technical Details
| Item | Value |
|------|-------|
| Protocol | CRSF (Crossfire Serial) |
| Baud rate | 420,000 |
| Channels | 16 (11-bit each, range 172–1811) |
| Wiring | UART2: GPIO 16 (RX), GPIO 17 (TX) |
| Voltage | 3.3V logic — direct connection, no level shifter |
| Library | AlfredoCRSF (mature, Arduino-compatible) |
| Update rate | 50–1000 Hz (configurable on transmitter) |

### RadioMaster Pocket Channel Map
| Channel | Source | Type |
|---------|--------|------|
| CH1 | Right stick L/R | Proportional |
| CH2 | Right stick U/D | Proportional |
| CH3 | Left stick U/D | Proportional |
| CH4 | Left stick L/R | Proportional |
| CH5 | SA switch | 2-position |
| CH6 | SB switch | 3-position |
| CH7 | SC switch | 3-position |
| CH8 | SD switch | 2-position |
| CH9 | SE button | Momentary |
| CH10 | S1 pot | Proportional |
| CH11-16 | User-assigned in EdgeTX | Configurable |

### Why it's easy
- Pure UART — no Bluetooth, no WiFi conflict, no radio contention
- Follows existing pattern: create `ELRSInput` class like `PS4Input`, cache state in struct, read in main loop
- AlfredoCRSF library handles all CRSF parsing
- 16 channels maps directly to 16 PWM outputs (1:1 if desired)

### Memory Impact
- AlfredoCRSF: ~2KB flash, ~256B RAM (UART buffer)
- ELRSState struct: ~36 bytes
- Total: negligible

### Implementation Effort: ~4–6 hours
1. Add `elrs_input.h/.cpp` (same pattern as `ps4_input`)
2. Add `INPUT_ELRS_CH1..CH16` enum values to `output_controller.h`
3. Add ELRS to web UI input dropdown (with CH1-16 sub-options)
4. Wire into `main.cpp` and `/api/raw-inputs`

### Risk: LOW
- No conflict with existing WiFi or BT Classic
- Proven library and protocol
- Can test independently with just a cheap ELRS receiver (~$10)

---

## 2. Embedded Scripting Language

### Verdict: FEASIBLE — Moderate risk, choose carefully

### Options Compared

| Language | RAM Footprint | Coexist w/ Arduino | Effort | 54KB Feasible | Notes |
|----------|---------------|-------------------|--------|---------------|-------|
| **TinyBasic** | 3–10 KB | Yes | Low | Excellent | Very limited syntax |
| **MyBasic** | 10–20 KB | Yes | Low | Good | Better features, FreeRTOS threads |
| **Lua (ESP-Arduino-Lua)** | 10–20 KB | Yes | Low | Marginal | Full language, proven on IoT |
| **Elk (JS)** | <1 KB core | Yes | Medium | Excellent | Minimal JS subset, 20KB flash |
| **MicroPython** | 30–50 KB | **No** | N/A | No | Replaces entire firmware |
| **Espruino (JS)** | 20–40 KB | Yes | Low | No | Too heavy |
| **Custom bytecode VM** | 2–10 KB | Yes | High | Excellent | Weeks of development |

### Recommendation: MyBasic (short-term) or Lua (long-term)

**MyBasic** is the sweet spot:
- Arduino library exists with ESP32 + FreeRTOS support
- Runs scripts in a background FreeRTOS task (non-blocking)
- Variables, functions, arrays, loops, conditions
- ~10–15KB overhead, leaving ~40KB for script data
- Scripts stored in LittleFS, loaded/run via web API

**Lua** if you need more power later:
- Tables, closures, coroutines, string manipulation
- Proven on millions of IoT devices (NodeMCU)
- ESP-Arduino-Lua library integrates directly
- Risk: tighter heap margin (~35KB remaining)

### Script Use Cases
```basic
' Example: sweep servo back and forth
FOR i = 0 TO 180 STEP 5
  PWM_SET 0, i
  DELAY 50
NEXT i
FOR i = 180 TO 0 STEP -5
  PWM_SET 0, i
  DELAY 50
NEXT i
```

### Architecture
```
Web UI                    ESP32
┌──────────┐   POST      ┌──────────────┐
│ Script   │ ───────────→ │ Save to      │
│ editor   │  /api/script │ LittleFS     │
│ (text)   │              │              │
└──────────┘              │ FreeRTOS     │
                          │ task runs    │
                          │ interpreter  │
                          │              │
                          │ Calls into   │
                          │ OutputCtrl   │
                          └──────────────┘
```

### Implementation Effort: ~8–12 hours
1. Add MyBasic or Lua library to `platformio.ini`
2. Create `script_engine.h/.cpp` — init interpreter, register native functions (PWM_SET, DELAY, READ_INPUT, etc.)
3. Run interpreter in FreeRTOS task (priority 1, below LVGL/output tasks)
4. Add API: `POST /api/script-run`, `POST /api/script-save`, `GET /api/script-list`
5. Add simple text editor to web UI (textarea + Run/Stop buttons)

### Risk: MODERATE
- Heap pressure: needs careful profiling under WiFi + BT + script load
- Script errors could crash the system (need error handling / watchdog)
- Performance: interpreted scripts at 50Hz is fine for servo control, not for tight timing

---

## 3. Block-Based Visual Programming UI (Blockly)

### Verdict: FEASIBLE — Higher effort, great UX payoff

### Architecture Options

| Approach | Flash Cost | Offline | Effort | Recommendation |
|----------|-----------|---------|--------|----------------|
| **A. Blockly from CDN** | ~0 KB | No (needs internet) | Medium | **Primary choice** |
| **B. Blockly bundled in flash** | ~1 MB | Yes | Medium | Fallback if offline needed |
| **C. Custom drag-drop UI** | ~20 KB | Yes | High | Only if Blockly is overkill |
| **D. Blockly on PC, REST to ESP32** | ~0 KB | N/A | Low | Awkward UX |

### Recommended: Option A — Blockly from CDN

Load Blockly JS (~200KB gzipped) from jsDelivr CDN. The ESP32 serves only a small HTML page with custom block definitions (~5KB). Blockly generates JSON action sequences that are sent to the ESP32 via REST API.

```
Browser                              ESP32
┌─────────────────────────┐          ┌─────────────────────┐
│ Blockly (from CDN)      │          │                     │
│ Custom PWM blocks (5KB) │  POST    │ Parse JSON sequence │
│ JSON generator          │ ───────→ │ Execute actions     │
│                         │          │ Store in LittleFS   │
└─────────────────────────┘          └─────────────────────┘
```

### Custom Blocks Needed

**Output blocks:**
- `Set channel [0-15] to angle [0-180]`
- `Ramp channel [0-15] from [0] to [180] over [1000] ms`
- `Set all channels to [90]`

**Timing blocks:**
- `Wait [500] ms`
- `Wait until input [CH1] > [128]`

**Control flow (Blockly built-in):**
- `Repeat [10] times { ... }`
- `While [condition] { ... }`
- `If [input > 128] then { ... } else { ... }`

**Input blocks:**
- `Read ELRS channel [1-16]` → value 0–180
- `Read PS4 [LX/LY/RX/RY/L2/R2]` → value
- `Read analog input [0-3]` → value

### Code Generation Target: JSON

Blockly generates a JSON action list:
```json
{
  "name": "sweep-test",
  "actions": [
    {"type": "loop", "count": 5, "body": [
      {"type": "pwm_set", "channel": 0, "angle": 0},
      {"type": "delay_ms", "ms": 500},
      {"type": "pwm_ramp", "channel": 0, "from": 0, "to": 180, "duration_ms": 1000},
      {"type": "delay_ms", "ms": 500}
    ]}
  ]
}
```

ESP32 parses this with ArduinoJson and executes it in a FreeRTOS task.

### Synergy with Scripting Language

If both Blockly and a scripting language are implemented, Blockly can generate script code (MyBasic/Lua) instead of JSON. This gives the best of both worlds:
- Visual programming for beginners
- Text editing for power users
- Same interpreter backend for both

### Implementation Effort: ~12–16 hours
1. Create Blockly page in web UI (load from CDN, define custom blocks)
2. Build JSON code generator for custom blocks
3. Build JSON sequence executor on ESP32 (FreeRTOS task)
4. Add save/load/list/delete API endpoints
5. Add Run/Stop controls
6. Test with various block combinations

### Risk: MODERATE
- CDN dependency (mitigated by Option B fallback)
- JSON sequences can get large — need to handle chunked upload (already have this pattern for curve-play)
- Complex sequences need careful error handling (infinite loops, missing channels)

---

## Implementation Priority & Timeline

### Recommended Order

```
Phase 1: ELRS Input              ← 4–6 hours, low risk, immediate value
    ↓
Phase 2: Script Engine           ← 8–12 hours, moderate risk
    ↓
Phase 3: Blockly Visual Editor   ← 12–16 hours, builds on Phase 2
```

### Why this order
1. **ELRS** is standalone — adds a new input source with no architectural changes
2. **Scripting** establishes the execution engine that Blockly will target
3. **Blockly** is the UI layer on top of the scripting engine

### Total estimated effort: 24–34 hours across all three phases

---

## Memory Budget After All Features

| Component | Heap Usage | Flash Usage |
|-----------|-----------|-------------|
| Current system (WiFi+BT+PCA9685+LittleFS) | ~266 KB | ~54.5% |
| + ELRS (AlfredoCRSF + UART buffer) | +0.3 KB | +2 KB |
| + MyBasic interpreter | +10–15 KB | +30 KB |
| + JSON sequence executor | +2 KB | +5 KB |
| + Blockly page (CDN, only custom defs) | +0 KB | +5 KB |
| **Estimated total** | **~280–283 KB** | **~56%** |
| **Remaining** | **~38–41 KB free** | **~44% free** |

All three features fit comfortably within the ESP32's resources.

---

## Open Questions

1. **ELRS receiver model**: Which ELRS receiver do you have / plan to buy? (e.g. BetaFPV EP2, HappyModel EP1)
2. **Offline requirement**: Does the web UI need to work without internet? (affects Blockly CDN vs bundled decision)
3. **Script complexity**: Do you need just sequences (linear actions) or full programming (loops, conditions, variables)?
4. **Mobile UI**: Should the Blockly editor work on phone/tablet, or desktop only?

---

## References

- AlfredoCRSF library: https://github.com/AlfredoSystems/AlfredoCRSF
- CRSF protocol spec: https://github.com/tbs-fpv/tbs-crsf-spec
- RadioMaster Pocket: https://radiomasterrc.com/products/pocket-radio-controller-m2
- MyBasic for ESP32: https://github.com/EternityForest/mybasic_esp32
- ESP-Arduino-Lua: https://github.com/sfranzyshen/ESP-Arduino-Lua
- Google Blockly: https://developers.google.com/blockly
- Blockly CDN (jsDelivr): https://cdn.jsdelivr.net/npm/blockly/
- SenseBox Blockly (reference impl): https://github.com/sensebox/ardublockly-1
- Elk JS engine (ultra-light): https://github.com/cesanta/elk
