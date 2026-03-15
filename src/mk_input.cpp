// MouldKing BLE motor controller — Bluedroid BLE GAP implementation.
// MK 6.0 protocol and crypto from MouldKingino (MIT license).
// Uses Bluedroid BLE GAP advertising instead of NimBLE, so it coexists
// with PS4 Classic BT on the same Bluedroid stack.

#include "mk_input.h"
#include "ps4_input.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

// ── MK60 protocol constants ─────────────────────────────────────────
static const uint8_t MK60_CONNECT[] = {0x6D, 0x7B, 0xA7, 0x80, 0x80, 0x80, 0x80, 0x92};
static const uint8_t MK60_BASE[]    = {0x61, 0x7B, 0xA7, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x9E};
static const uint8_t MK_SEED[]   = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5};
static const uint8_t MK_HEADER[] = {0x71, 0x0F, 0x55};
static const uint16_t MK_COMPANY_ID = 0xFFF0;
static const int ENCRYPTED_PACKET_LEN = 24;
static const int ENCRYPTED_HEADER_OFFSET = 15;
static const uint8_t CTX_VAL1 = 0x3F;
static const uint8_t CTX_VAL2 = 0x25;

// ── MK crypto (from MouldKingino MKCryptoHelper, MIT) ────────────────
static uint8_t reverseByte(uint8_t v) {
    return (uint8_t)(((v * 0x0802U & 0x22110U) | (v * 0x8020U & 0x88440U)) * 0x10101U >> 16);
}
static uint16_t reverseWord(uint16_t v) {
    v = (uint16_t)(((v & 0xAAAA) >> 1) | ((v & 0x5555) << 1));
    v = (uint16_t)(((v & 0xCCCC) >> 2) | ((v & 0x3333) << 2));
    v = (uint16_t)(((v & 0xF0F0) >> 4) | ((v & 0x0F0F) << 4));
    return (uint16_t)((v >> 8) | (v << 8));
}
static uint16_t checkCRC16(const uint8_t* a1, int a1len, const uint8_t* a2, int a2len) {
    int r = 0xFFFF;
    for (int i = a1len - 1; i >= 0; i--) {
        r ^= a1[i] << 8;
        for (int j = 0; j < 8; j++) r = (r & 0x8000) ? (r << 1) ^ 0x1021 : r << 1;
    }
    for (int i = 0; i < a2len; i++) {
        r ^= reverseByte(a2[i]) << 8;
        for (int j = 0; j < 8; j++) r = (r & 0x8000) ? (r << 1) ^ 0x1021 : r << 1;
    }
    return (uint16_t)(reverseWord((uint16_t)r) ^ 0xFFFF);
}
static void whiteningInit(uint8_t val, uint8_t* ctx) {
    ctx[0] = 1;
    for (int i = 1; i <= 6; i++) ctx[i] = (val >> (6 - i)) & 1;
}
static uint8_t whiteningOutput(uint8_t* ctx) {
    uint8_t v3 = ctx[3], v6 = ctx[6];
    ctx[3] = ctx[2]; ctx[2] = ctx[1]; ctx[1] = ctx[0]; ctx[0] = ctx[6];
    ctx[6] = ctx[5]; ctx[5] = ctx[4]; ctx[4] = (uint8_t)(v3 ^ v6);
    return ctx[0];
}
static void whiteningEncode(uint8_t* data, int start, int len, uint8_t* ctx) {
    for (int i = 0; i < len; i++) {
        uint8_t b = data[start + i]; int r = 0;
        for (int bit = 0; bit < 8; bit++) r |= ((whiteningOutput(ctx) ^ ((b >> bit) & 1)) << bit);
        data[start + i] = (uint8_t)r;
    }
}
static int mkEncrypt(const uint8_t* payload, int payloadLen, uint8_t* out) {
    const int seedLen = sizeof(MK_SEED), hdrLen = sizeof(MK_HEADER), crcLen = 2;
    int seedOff = ENCRYPTED_HEADER_OFFSET + hdrLen;
    int dataOff = seedOff + seedLen, crcOff = dataOff + payloadLen, bufLen = crcOff + crcLen;
    uint8_t buf[64]; memset(buf, 0, sizeof(buf));
    memcpy(&buf[ENCRYPTED_HEADER_OFFSET], MK_HEADER, hdrLen);
    for (int i = 0; i < seedLen; i++) buf[seedOff + i] = MK_SEED[seedLen - 1 - i];
    for (int i = 0; i < hdrLen + seedLen; i++)
        buf[ENCRYPTED_HEADER_OFFSET + i] = reverseByte(buf[ENCRYPTED_HEADER_OFFSET + i]);
    memcpy(&buf[dataOff], payload, payloadLen);
    uint16_t crc = checkCRC16(MK_SEED, seedLen, payload, payloadLen);
    memcpy(&buf[crcOff], &crc, crcLen);
    uint8_t ctx1[7]; whiteningInit(CTX_VAL1, ctx1);
    whiteningEncode(buf, seedOff, seedLen + payloadLen + crcLen, ctx1);
    uint8_t ctx2[7]; whiteningInit(CTX_VAL2, ctx2);
    whiteningEncode(buf, 0, bufLen, ctx2);
    int resultLen = hdrLen + seedLen + payloadLen + crcLen;
    memcpy(out, &buf[ENCRYPTED_HEADER_OFFSET], resultLen);
    for (int i = resultLen; i < ENCRYPTED_PACKET_LEN; i++) out[i] = (uint8_t)(i + 1);
    return ENCRYPTED_PACKET_LEN;
}

// ── Bluedroid BLE GAP advertising ────────────────────────────────────
static uint8_t s_channelData[10];
static esp_ble_adv_params_t s_advParams = {
    .adv_int_min = 0x20, .adv_int_max = 0x20,
    .adv_type = ADV_TYPE_NONCONN_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {0}, .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
static void mkGapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT)
        esp_ble_gap_start_advertising(&s_advParams);
}
static void mkAdvertise(const uint8_t* payload, int payloadLen) {
    uint8_t encrypted[ENCRYPTED_PACKET_LEN];
    mkEncrypt(payload, payloadLen, encrypted);
    uint8_t raw[31]; int pos = 0;
    raw[pos++] = 0x02; raw[pos++] = 0x01; raw[pos++] = 0x06;
    raw[pos++] = (uint8_t)(ENCRYPTED_PACKET_LEN + 3); raw[pos++] = 0xFF;
    memcpy(&raw[pos], &MK_COMPANY_ID, 2); pos += 2;
    memcpy(&raw[pos], encrypted, ENCRYPTED_PACKET_LEN); pos += ENCRYPTED_PACKET_LEN;
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data_raw(raw, pos);
}

// ── MkInput implementation ──────────────────────────────────────────

void MkInput::init(PS4Input* ps4) {
    _ps4 = ps4;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        _values[i] = 90.0f;
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
    if (!btStarted()) { if (!btStart()) { Serial.println("[MK] ERROR: btStart() failed"); return; } }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED)
        if (esp_bluedroid_init() != ESP_OK) { Serial.println("[MK] ERROR: bluedroid init failed"); return; }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
        if (esp_bluedroid_enable() != ESP_OK) { Serial.println("[MK] ERROR: bluedroid enable failed"); return; }
    esp_ble_gap_register_callback(mkGapCallback);
    Serial.println("[MK] Bluedroid BLE initialized");
}

void MkInput::connect() {
    Serial.printf("[MK] Broadcasting connect for %d ms — power on MK hub now!\n", MK_CONNECT_MS);
    mkAdvertise(MK60_CONNECT, sizeof(MK60_CONNECT));
    vTaskDelay(pdMS_TO_TICKS(MK_CONNECT_MS));
    _connected = true;
    memcpy(s_channelData, MK60_BASE, sizeof(MK60_BASE));
    mkAdvertise(s_channelData, sizeof(s_channelData));
    Serial.printf("[MK] Connect complete, %d channels ready.\n", NUM_MK_CHANNELS);
}

void MkInput::disconnect() {
    esp_ble_gap_stop_advertising();
    _connected = false;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        _values[i] = 90.0f;
        _inputSrc[i] = INPUT_MANUAL;
        _curvePlaying[i] = false;
    }
    memcpy(s_channelData, MK60_BASE, sizeof(MK60_BASE));
    Serial.println("[MK] Disconnected, channels reset.");
}

void MkInput::setValue(uint8_t ch, float value) {
    if (ch >= NUM_MK_CHANNELS) return;
    _values[ch] = constrain(value, 0.0f, 180.0f);
}

float MkInput::getValue(uint8_t ch) const {
    if (ch >= NUM_MK_CHANNELS) return 90.0f;
    return _values[ch];
}

void MkInput::setInputSource(uint8_t ch, InputSource src) {
    if (ch >= NUM_MK_CHANNELS) return;
    _inputSrc[ch] = src;
    // Stop curve when input changes
    if (_curvePlaying[ch]) _curvePlaying[ch] = false;
}

InputSource MkInput::getInputSource(uint8_t ch) const {
    if (ch >= NUM_MK_CHANNELS) return INPUT_MANUAL;
    return _inputSrc[ch];
}

void MkInput::setPotIndex(uint8_t ch, uint8_t potIdx) {
    if (ch < NUM_MK_CHANNELS) _potIndex[ch] = potIdx;
}

uint8_t MkInput::getPotIndex(uint8_t ch) const {
    return ch < NUM_MK_CHANNELS ? _potIndex[ch] : 0;
}

// ── Curve playback ──────────────────────────────────────────────────

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

void MkInput::stopCurve(uint8_t ch) {
    if (ch < NUM_MK_CHANNELS) _curvePlaying[ch] = false;
}

void MkInput::stopAllCurves() {
    for (int i = 0; i < NUM_MK_CHANNELS; i++) _curvePlaying[i] = false;
}

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

void MkInput::update() {
    if (!_connected) return;
    unsigned long now = millis();
    if (now - _lastUpdateMs < MK_UPDATE_MS) return;
    _lastUpdateMs = now;

    // Process curves and input routing per channel
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        // Curve playback takes priority
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

        // Route PS4 inputs
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

        // Route pot input
        if (_inputSrc[i] == INPUT_POT && _potIndex[i] < NUM_POTS) {
            int raw = analogRead(POT_PINS[_potIndex[i]]);
            _values[i] = (float)map(raw, 0, 4095, 0, 180);
        }
    }

    // Update channel data bytes, track if anything changed
    bool changed = false;
    for (int i = 0; i < NUM_MK_CHANNELS; i++) {
        double speed = (_values[i] - 90.0) / 90.0;
        uint8_t newVal = (uint8_t)(speed * 127.0 + 128.0);
        if (s_channelData[3 + i] != newVal) {
            s_channelData[3 + i] = newVal;
            changed = true;
        }
    }

    // Only re-advertise on value change or every 500ms as keepalive
    static unsigned long lastAdvMs = 0;
    if (changed || (now - lastAdvMs >= 500)) {
        mkAdvertise(s_channelData, sizeof(s_channelData));
        lastAdvMs = now;
    }
}
