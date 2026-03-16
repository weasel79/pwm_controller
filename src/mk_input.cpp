// mk_input.cpp — MouldKing BLE motor controller (6 channels A-F)
//
// Implements the MK 6.0 BLE advertising protocol using ESP32 Bluedroid BLE GAP.
// The MK hub is a passive BLE listener — it receives motor commands via BLE
// advertisement packets containing encrypted payloads (CRC16 + LFSR whitening).
//
// Protocol and crypto adapted from MouldKingino by Dmitry Akulov (MIT license).
// https://github.com/pink0D/MouldKingino
//
// Key design decisions:
// - Uses Bluedroid BLE GAP API (not NimBLE) so it coexists with PS4 Classic BT
//   on the same Bluedroid stack. NimBLE and Bluedroid cannot run simultaneously.
// - BLE GAP is initialized lazily on first connect() call to avoid memory/CPU
//   overhead when MK is not in use (~10KB heap saved at idle).
// - No GAP callback registered — advertising is done synchronously with a 5ms
//   delay between config and start, reducing BLE event processing overhead.
// - Advertising stops entirely when all motors are neutral (speed=0), freeing
//   the 2.4GHz radio for WiFi. Restarts when a motor value changes.
// - connect() runs in a FreeRTOS background task so it doesn't block the web
//   server during the 3-second pairing broadcast.
// - Values 0-180 map to MK speed -1.0..+1.0 (90 = stopped).
//   Formula: speed = (value - 90) / 90, MK byte = speed * 127 + 128.

#include "mk_input.h"
#include "ps4_input.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

// ── MK60 protocol constants ─────────────────────────────────────────
// Connect telegram: broadcast during pairing window so hub starts listening
static const uint8_t MK60_CONNECT[] = {0x6D, 0x7B, 0xA7, 0x80, 0x80, 0x80, 0x80, 0x92};

// Base motor data telegram: bytes [3..8] are channels A-F (128 = stopped)
// Byte [0] = module ID (0x61=module1), byte [9] = checksum complement
static const uint8_t MK60_BASE[] = {0x61, 0x7B, 0xA7, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x9E};

// Encryption constants
static const uint8_t MK_SEED[]   = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5};  // XOR seed for CRC
static const uint8_t MK_HEADER[] = {0x71, 0x0F, 0x55};               // RF packet header
static const uint16_t MK_COMPANY_ID = 0xFFF0;   // BLE manufacturer ID
static const int ENCRYPTED_PACKET_LEN = 24;      // Fixed output size after encryption
static const int ENCRYPTED_HEADER_OFFSET = 15;   // Position of header in work buffer
static const uint8_t CTX_VAL1 = 0x3F;            // Whitening seed 1
static const uint8_t CTX_VAL2 = 0x25;            // Whitening seed 2

// ── MK crypto functions ──────────────────────────────────────────────
// All crypto from MouldKingino MKCryptoHelper (MIT license).
// The MK protocol encrypts every BLE advertisement payload using:
// 1. CRC16 checksum over seed + data (custom polynomial 0x1021)
// 2. Two passes of LFSR (linear feedback shift register) whitening
// The result is a 24-byte encrypted payload broadcast as manufacturer data.

// Reverse all bits in a byte (MSB <-> LSB)
static uint8_t reverseByte(uint8_t v) {
    return (uint8_t)(((v * 0x0802U & 0x22110U) | (v * 0x8020U & 0x88440U)) * 0x10101U >> 16);
}

// Reverse all bits in a 16-bit word
static uint16_t reverseWord(uint16_t v) {
    v = (uint16_t)(((v & 0xAAAA) >> 1) | ((v & 0x5555) << 1));
    v = (uint16_t)(((v & 0xCCCC) >> 2) | ((v & 0x3333) << 2));
    v = (uint16_t)(((v & 0xF0F0) >> 4) | ((v & 0x0F0F) << 4));
    return (uint16_t)((v >> 8) | (v << 8));
}

// CRC16 with custom processing: seed array reversed, data array bit-reversed
static uint16_t checkCRC16(const uint8_t* a1, int a1len, const uint8_t* a2, int a2len) {
    int r = 0xFFFF;
    // Process seed array in reverse order
    for (int i = a1len - 1; i >= 0; i--) {
        r ^= a1[i] << 8;
        for (int j = 0; j < 8; j++) r = (r & 0x8000) ? (r << 1) ^ 0x1021 : r << 1;
    }
    // Process data array with bit-reversed input bytes
    for (int i = 0; i < a2len; i++) {
        r ^= reverseByte(a2[i]) << 8;
        for (int j = 0; j < 8; j++) r = (r & 0x8000) ? (r << 1) ^ 0x1021 : r << 1;
    }
    return (uint16_t)(reverseWord((uint16_t)r) ^ 0xFFFF);
}

// Initialize 7-element LFSR context from a seed value
static void whiteningInit(uint8_t val, uint8_t* ctx) {
    ctx[0] = 1;
    for (int i = 1; i <= 6; i++) ctx[i] = (val >> (6 - i)) & 1;
}

// Generate one bit from the LFSR and shift the register
static uint8_t whiteningOutput(uint8_t* ctx) {
    uint8_t v3 = ctx[3], v6 = ctx[6];
    ctx[3] = ctx[2]; ctx[2] = ctx[1]; ctx[1] = ctx[0]; ctx[0] = ctx[6];
    ctx[6] = ctx[5]; ctx[5] = ctx[4]; ctx[4] = (uint8_t)(v3 ^ v6);
    return ctx[0];
}

// XOR each bit in data[start..start+len] with LFSR output (whitening)
static void whiteningEncode(uint8_t* data, int start, int len, uint8_t* ctx) {
    for (int i = 0; i < len; i++) {
        uint8_t b = data[start + i]; int r = 0;
        for (int bit = 0; bit < 8; bit++) r |= ((whiteningOutput(ctx) ^ ((b >> bit) & 1)) << bit);
        data[start + i] = (uint8_t)r;
    }
}

// Encrypt a raw MK telegram into a 24-byte BLE advertising payload.
// Steps: assemble header+seed+data+CRC in work buffer, apply two whitening
// passes with different seeds, copy result, pad to 24 bytes.
// Returns ENCRYPTED_PACKET_LEN (always 24).
static int mkEncrypt(const uint8_t* payload, int payloadLen, uint8_t* out) {
    const int seedLen = sizeof(MK_SEED), hdrLen = sizeof(MK_HEADER), crcLen = 2;
    int seedOff = ENCRYPTED_HEADER_OFFSET + hdrLen;
    int dataOff = seedOff + seedLen, crcOff = dataOff + payloadLen, bufLen = crcOff + crcLen;

    // Build work buffer: [padding][header][seed_reversed][data][crc16]
    uint8_t buf[64]; memset(buf, 0, sizeof(buf));
    memcpy(&buf[ENCRYPTED_HEADER_OFFSET], MK_HEADER, hdrLen);
    for (int i = 0; i < seedLen; i++) buf[seedOff + i] = MK_SEED[seedLen - 1 - i];

    // Bit-reverse header and seed bytes
    for (int i = 0; i < hdrLen + seedLen; i++)
        buf[ENCRYPTED_HEADER_OFFSET + i] = reverseByte(buf[ENCRYPTED_HEADER_OFFSET + i]);

    // Append raw motor data and CRC
    memcpy(&buf[dataOff], payload, payloadLen);
    uint16_t crc = checkCRC16(MK_SEED, seedLen, payload, payloadLen);
    memcpy(&buf[crcOff], &crc, crcLen);

    // Whitening pass 1: seed+data+crc region
    uint8_t ctx1[7]; whiteningInit(CTX_VAL1, ctx1);
    whiteningEncode(buf, seedOff, seedLen + payloadLen + crcLen, ctx1);

    // Whitening pass 2: entire buffer
    uint8_t ctx2[7]; whiteningInit(CTX_VAL2, ctx2);
    whiteningEncode(buf, 0, bufLen, ctx2);

    // Copy encrypted result and pad remaining bytes
    int resultLen = hdrLen + seedLen + payloadLen + crcLen;
    memcpy(out, &buf[ENCRYPTED_HEADER_OFFSET], resultLen);
    for (int i = resultLen; i < ENCRYPTED_PACKET_LEN; i++) out[i] = (uint8_t)(i + 1);
    return ENCRYPTED_PACKET_LEN;
}

// ── Bluedroid BLE GAP advertising ────────────────────────────────────
// Persistent channel data buffer — bytes [3..8] hold motor speeds for A-F.
// Updated in-place by update(), encrypted and broadcast by mkAdvertise().
static uint8_t s_channelData[10];

// BLE advertising parameters.
// ADV_TYPE_NONCONN_IND = non-connectable undirected (broadcast only).
// Interval 0x40 = 64 * 0.625ms = 40ms between advertisements.
static esp_ble_adv_params_t s_advParams = {
    .adv_int_min = 0x40, .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_NONCONN_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0}, .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Encrypt a motor data telegram and broadcast it as a BLE advertisement.
// Uses synchronous flow (stop → set data → delay → start) instead of a GAP
// callback to avoid BLE event processing overhead that interferes with WiFi.
// The 5ms delay gives the BLE controller time to process the raw adv data
// before starting the advertisement.
static void mkAdvertise(const uint8_t* payload, int payloadLen) {
    // Encrypt the motor telegram
    uint8_t encrypted[ENCRYPTED_PACKET_LEN];
    mkEncrypt(payload, payloadLen, encrypted);

    // Build raw BLE advertising data: [Flags AD] [Manufacturer Data AD]
    // Total = 3 (flags) + 3 (mfr header) + 24 (encrypted) = 30 bytes
    uint8_t raw[31]; int pos = 0;
    raw[pos++] = 0x02; raw[pos++] = 0x01; raw[pos++] = 0x06;  // Flags: LE General + BR/EDR not supported
    raw[pos++] = (uint8_t)(ENCRYPTED_PACKET_LEN + 3);          // Mfr data length
    raw[pos++] = 0xFF;                                          // Type: Manufacturer Specific
    memcpy(&raw[pos], &MK_COMPANY_ID, 2); pos += 2;            // Company ID 0xFFF0 (little-endian)
    memcpy(&raw[pos], encrypted, ENCRYPTED_PACKET_LEN); pos += ENCRYPTED_PACKET_LEN;

    // Stop current advertising, configure new data, restart
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data_raw(raw, pos);
    vTaskDelay(pdMS_TO_TICKS(5));
    esp_ble_gap_start_advertising(&s_advParams);
}

// ── MkInput implementation ──────────────────────────────────────────

// Track whether BLE GAP has been initialized (deferred until first connect)
static bool s_bleGapReady = false;

// Initialize MK controller state. Does NOT start BLE — that's deferred to
// connect() to avoid wasting ~10KB heap + CPU when MK isn't being used.
// ps4: pointer to PS4Input for reading controller state when MK channels
//       have PS4 input sources assigned.
void MkInput::init(PS4Input* ps4) {
    _ps4 = ps4;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        _values[i] = 90.0f;          // All motors stopped
        _inputSrc[i] = INPUT_MANUAL;
        _potIndex[i] = 0;
        _curveData[i] = nullptr;
        _curveLen[i] = 0;
        _curveDur[i] = 0;
        _curveLoop[i] = false;
        _curvePlaying[i] = false;
        _curveStartMs[i] = 0;
    }
    memcpy(s_channelData, MK60_BASE, sizeof(MK60_BASE));
    Serial.println("[MK] Ready (BLE deferred until connect)");
}

// Initialize Bluedroid BLE GAP on first use. Called from the connect task.
// If PS4 has already initialized Bluedroid (btStart + bluedroid_init/enable),
// those calls are skipped (status checks prevent double-init).
static void mkEnsureBleGap() {
    if (s_bleGapReady) return;
    if (!btStarted()) {
        if (!btStart()) { Serial.println("[MK] ERROR: btStart() failed"); return; }
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        if (esp_bluedroid_init() != ESP_OK) { Serial.println("[MK] ERROR: bluedroid init failed"); return; }
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
        if (esp_bluedroid_enable() != ESP_OK) { Serial.println("[MK] ERROR: bluedroid enable failed"); return; }
    }
    s_bleGapReady = true;
    Serial.println("[MK] BLE GAP initialized");
}

// FreeRTOS task for non-blocking MK connect. Runs in a background task so
// the 3-second pairing broadcast doesn't block the AsyncWebServer task.
// After broadcasting the connect telegram, stops advertising and sets
// _connected = true so update() starts sending motor data.
void mkConnectTask(void* param) {
    MkInput* self = (MkInput*)param;
    mkEnsureBleGap();
    Serial.printf("[MK] Broadcasting connect for %d ms — power on MK hub now!\n", MK_CONNECT_MS);
    mkAdvertise(MK60_CONNECT, sizeof(MK60_CONNECT));
    vTaskDelay(pdMS_TO_TICKS(MK_CONNECT_MS));
    esp_ble_gap_stop_advertising();
    self->_connected = true;
    Serial.printf("[MK] Connect complete, %d channels ready.\n", NUM_MK_CHANNELS);
    vTaskDelete(NULL);
}

// Start MK hub connection. Spawns a background task that broadcasts the
// pairing advertisement for MK_CONNECT_MS (3 seconds by default).
// The hub must be powered on during this window.
void MkInput::connect() {
    if (_connected) return;
    memcpy(s_channelData, MK60_BASE, sizeof(MK60_BASE));
    xTaskCreate(mkConnectTask, "mk_conn", 2048, this, 1, NULL);
}

// Disconnect from MK hub. Stops BLE advertising and resets all channels.
void MkInput::disconnect() {
    esp_ble_gap_stop_advertising();
    _connected = false;
    _advertising = false;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        _values[i] = 90.0f;
        _inputSrc[i] = INPUT_MANUAL;
        _curvePlaying[i] = false;
    }
    memcpy(s_channelData, MK60_BASE, sizeof(MK60_BASE));
    Serial.println("[MK] Disconnected, channels reset.");
}

// Set motor value for channel 0-5. Range 0-180 where 90 = stopped.
void MkInput::setValue(uint8_t ch, float value) {
    if (ch >= NUM_MK_CHANNELS) return;
    _values[ch] = constrain(value, 0.0f, 180.0f);
}

// Get current motor value for channel 0-5.
float MkInput::getValue(uint8_t ch) const {
    if (ch >= NUM_MK_CHANNELS) return 90.0f;
    return _values[ch];
}

// Set input source for a MK channel. Stops any active curve playback.
void MkInput::setInputSource(uint8_t ch, InputSource src) {
    if (ch >= NUM_MK_CHANNELS) return;
    _inputSrc[ch] = src;
    if (_curvePlaying[ch]) _curvePlaying[ch] = false;
}

InputSource MkInput::getInputSource(uint8_t ch) const {
    if (ch >= NUM_MK_CHANNELS) return INPUT_MANUAL;
    return _inputSrc[ch];
}

// Set which physical potentiometer (0-3) drives this channel when input=pot
void MkInput::setPotIndex(uint8_t ch, uint8_t potIdx) {
    if (ch < NUM_MK_CHANNELS) _potIndex[ch] = potIdx;
}

uint8_t MkInput::getPotIndex(uint8_t ch) const {
    return ch < NUM_MK_CHANNELS ? _potIndex[ch] : 0;
}

// ── Curve playback (envelope/sequence) ──────────────────────────────
// Same pattern as OutputController: browser sends all curve points once,
// ESP interpolates locally at MK_UPDATE_MS intervals (~50ms / 20Hz).

// Start playing a curve on a MK channel. Points are copied to heap.
// count: number of CurvePoint entries (must be >= 2)
// duration: total curve time in seconds
// loop: whether to loop continuously
void MkInput::playCurve(uint8_t ch, const CurvePoint* points, uint16_t count, float duration, bool loop) {
    if (ch >= NUM_MK_CHANNELS || count < 2) return;
    if (_curveData[ch]) { free(_curveData[ch]); _curveData[ch] = nullptr; }
    _curveData[ch] = (CurvePoint*)malloc(count * sizeof(CurvePoint));
    if (!_curveData[ch]) return;
    memcpy(_curveData[ch], points, count * sizeof(CurvePoint));
    _curveLen[ch] = count;
    _curveDur[ch] = duration;
    _curveLoop[ch] = loop;
    _curvePlaying[ch] = true;
    _curveStartMs[ch] = millis();
}

// Stop curve playback on a single channel
void MkInput::stopCurve(uint8_t ch) {
    if (ch < NUM_MK_CHANNELS) _curvePlaying[ch] = false;
}

// Stop curve playback on all MK channels
void MkInput::stopAllCurves() {
    for (int i = 0; i < NUM_MK_CHANNELS; i++) _curvePlaying[i] = false;
}

// Linear interpolation between curve points at time t.
// Returns the angle value at the given time position.
float MkInput::_interpolateCurve(uint8_t ch, float t) const {
    const CurvePoint* pts = _curveData[ch];
    uint16_t len = _curveLen[ch];
    if (len == 0) return _values[ch];
    if (len == 1) return pts[0].a;
    if (t <= pts[0].t) return pts[0].a;
    if (t >= pts[len - 1].t) return pts[len - 1].a;
    for (uint16_t i = 0; i < len - 1; i++) {
        if (t >= pts[i].t && t <= pts[i + 1].t) {
            float span = pts[i + 1].t - pts[i].t;
            if (span < 0.0001f) return pts[i].a;
            return pts[i].a + (t - pts[i].t) / span * (pts[i + 1].a - pts[i].a);
        }
    }
    return pts[len - 1].a;
}

// ── Main update loop ────────────────────────────────────────────────
// Called from loop() every iteration. Throttled to MK_UPDATE_MS (50ms).
// Processes input routing (curves > PS4 > pot > manual), converts values
// to MK byte format, and broadcasts via BLE when values change.
void MkInput::update() {
    if (!_connected) return;

    // Throttle updates to MK_UPDATE_MS interval
    unsigned long now = millis();
    if (now - _lastUpdateMs < MK_UPDATE_MS) return;
    _lastUpdateMs = now;

    // ── Input routing per channel ────────────────────────────────────
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        // Curve playback has highest priority — overrides all other inputs
        if (_curvePlaying[i] && _curveData[i] && _curveLen[i] >= 2) {
            float elapsed = (now - _curveStartMs[i]) / 1000.0f;
            if (_curveLoop[i]) {
                elapsed = fmodf(elapsed, _curveDur[i]);
            } else if (elapsed > _curveDur[i]) {
                _curvePlaying[i] = false;
                continue;
            }
            _values[i] = _interpolateCurve(i, elapsed);
            continue;
        }

        // PS4 controller input: map stick/trigger/button to motor speed
        if (_ps4 && _inputSrc[i] >= INPUT_PS4_LX && _inputSrc[i] <= INPUT_PS4_R1) {
            const PS4State& s = _ps4->getState();
            switch (_inputSrc[i]) {
                case INPUT_PS4_LX: _values[i] = (float)map(s.lx, -128, 127, 0, 180); break;
                case INPUT_PS4_LY: _values[i] = (float)map(s.ly, -128, 127, 0, 180); break;
                case INPUT_PS4_RX: _values[i] = (float)map(s.rx, -128, 127, 0, 180); break;
                case INPUT_PS4_RY: _values[i] = (float)map(s.ry, -128, 127, 0, 180); break;
                case INPUT_PS4_L2: _values[i] = (float)map(s.l2, 0, 255, 90, 180);   break;
                case INPUT_PS4_R2: _values[i] = (float)map(s.r2, 0, 255, 90, 180);   break;
                case INPUT_PS4_CROSS:    _values[i] = s.cross    ? 180.0f : 90.0f; break;
                case INPUT_PS4_CIRCLE:   _values[i] = s.circle   ? 180.0f : 90.0f; break;
                case INPUT_PS4_SQUARE:   _values[i] = s.square   ? 180.0f : 90.0f; break;
                case INPUT_PS4_TRIANGLE: _values[i] = s.triangle ? 180.0f : 90.0f; break;
                case INPUT_PS4_L1:       _values[i] = s.l1       ? 180.0f : 90.0f; break;
                case INPUT_PS4_R1:       _values[i] = s.r1       ? 180.0f : 90.0f; break;
                default: break;
            }
        }

        // Potentiometer input: read ADC and map to 0-180
        if (_inputSrc[i] == INPUT_POT && _potIndex[i] < NUM_POTS) {
            int raw = analogRead(POT_PINS[_potIndex[i]]);
            _values[i] = (float)map(raw, 0, 4095, 0, 180);
        }
    }

    // ── Convert values to MK byte format and detect changes ─────────
    // Deadzone: values within ±2 of center (90) snap to exactly 90.
    // This prevents PS4 stick jitter from toggling advertising on/off.
    bool changed = false;
    bool allNeutral = true;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        float val = _values[i];
        if (fabsf(val - 90.0f) < 2.0f) val = 90.0f;   // snap to center deadzone
        double speed = (val - 90.0) / 90.0;             // -1.0 to +1.0
        uint8_t newVal = (uint8_t)(speed * 127.0 + 128.0); // 1=full reverse, 128=stop, 255=full forward
        if (s_channelData[3 + i] != newVal) {
            s_channelData[3 + i] = newVal;
            changed = true;
        }
        if (newVal != 128) allNeutral = false;
    }

    // ── Advertising control ──────────────────────────────────────────
    // When all motors are stopped and nothing changed, stop BLE advertising
    // entirely. This frees the 2.4GHz radio for WiFi, reducing TCP timeouts.
    if (allNeutral && !changed) {
        if (_advertising) {
            esp_ble_gap_stop_advertising();
            _advertising = false;
        }
        return;
    }

    // Send BLE advertisement on value change, or every 500ms as keepalive
    // while at least one motor is running
    static unsigned long lastAdvMs = 0;
    if (changed || (now - lastAdvMs >= 500)) {
        mkAdvertise(s_channelData, sizeof(s_channelData));
        lastAdvMs = now;
        _advertising = true;
    }
}
