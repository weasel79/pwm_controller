#include "output_controller.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <math.h>

static const char* _inputSourceName(InputSource s) {
    switch (s) {
        case INPUT_MANUAL: return "manual"; case INPUT_ENVELOPE: return "envelope";
        case INPUT_POT: return "pot"; case INPUT_PS4_LX: return "ps4_lx";
        case INPUT_PS4_LY: return "ps4_ly"; case INPUT_PS4_RX: return "ps4_rx";
        case INPUT_PS4_RY: return "ps4_ry"; case INPUT_PS4_L2: return "ps4_l2";
        case INPUT_PS4_R2: return "ps4_r2"; case INPUT_PS4_CROSS: return "ps4_cross";
        case INPUT_PS4_CIRCLE: return "ps4_circle"; case INPUT_PS4_SQUARE: return "ps4_square";
        case INPUT_PS4_TRIANGLE: return "ps4_tri"; case INPUT_PS4_L1: return "ps4_l1";
        case INPUT_PS4_R1: return "ps4_r1"; case INPUT_SEQUENCE: return "sequence";
        default: return "?";
    }
}

void OutputController::init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    _pca.begin();
    _pca.setOscillatorFrequency(25000000);
    _pca.setPWMFreq(OUTPUT_FREQ_HZ);

    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        _channels[i].channel = i;
        _channels[i].type = OUTPUT_SERVO;
        _channels[i].minUs = OUTPUT_DEFAULT_MIN_US;
        _channels[i].maxUs = OUTPUT_DEFAULT_MAX_US;
        _channels[i].currentValue = (float)OUTPUT_DEFAULT_VALUE;
        _channels[i].targetValue = (float)OUTPUT_DEFAULT_VALUE;
        _channels[i].enabled = true;
        _channels[i].inputSource = INPUT_MANUAL;
        _channels[i].potIndex = 0;
        _channels[i].curveData = nullptr;
        _channels[i].curveLen = 0;
        _channels[i].curvePlaying = false;
        snprintf(_channels[i].name, sizeof(_channels[i].name), "Output %d", i);
    }

    // Move all outputs to default position
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        _writePWM(i, (float)OUTPUT_DEFAULT_VALUE);
    }

    Serial.println("[Output] PCA9685 initialized, all outputs at default value");
}

void OutputController::setValue(uint8_t channel, float value) {
    if (channel >= NUM_OUTPUTS) return;
    value = constrain(value, 0.0f, 180.0f);

    _channels[channel].targetValue = value;

    if (_smoothSteps == 0) {
        _channels[channel].currentValue = value;
        _writePWM(channel, value);
    }

#if LOG_LEVEL >= 2
    static unsigned long _lastTraceMs[NUM_OUTPUTS] = {};
    unsigned long now = millis();
    float oldVal = _channels[channel].currentValue;
    if (fabsf(oldVal - value) > 0.5f && (now - _lastTraceMs[channel] >= 1000)) {
        _lastTraceMs[channel] = now;
        Serial.printf("[%lu] SET -> ch%d: %.1f -> %.1f (src=%s)\n",
                      now, channel, oldVal, value,
                      _inputSourceName(_channels[channel].inputSource));
    }
#endif
}

uint8_t OutputController::getValue(uint8_t channel) const {
    if (channel >= NUM_OUTPUTS) return 0;
    return (uint8_t)roundf(_channels[channel].currentValue);
}

void OutputController::setAllValues(const uint8_t values[], uint8_t count) {
    uint8_t n = min((uint8_t)NUM_OUTPUTS, count);
    for (uint8_t i = 0; i < n; i++) {
        setValue(i, (float)values[i]);
    }
}

void OutputController::getAllValues(uint8_t outValues[], uint8_t count) const {
    uint8_t n = min((uint8_t)NUM_OUTPUTS, count);
    for (uint8_t i = 0; i < n; i++) {
        outValues[i] = (uint8_t)roundf(_channels[i].currentValue);
    }
}

const OutputChannel& OutputController::getChannel(uint8_t ch) const {
    return _channels[ch < NUM_OUTPUTS ? ch : 0];
}

void OutputController::setChannelRange(uint8_t channel, uint16_t minUs, uint16_t maxUs) {
    if (channel >= NUM_OUTPUTS) return;
    _channels[channel].minUs = minUs;
    _channels[channel].maxUs = maxUs;
}

void OutputController::setChannelName(uint8_t channel, const char* name) {
    if (channel >= NUM_OUTPUTS) return;
    strncpy(_channels[channel].name, name, sizeof(_channels[channel].name) - 1);
    _channels[channel].name[sizeof(_channels[channel].name) - 1] = '\0';
}

void OutputController::setChannelType(uint8_t channel, OutputType type) {
    if (channel >= NUM_OUTPUTS) return;
    _channels[channel].type = type;
    _updateFrequency();
}

void OutputController::setChannelInput(uint8_t channel, InputSource src) {
    if (channel >= NUM_OUTPUTS) return;
#if LOG_LEVEL >= 1
    InputSource old = _channels[channel].inputSource;
#endif
    _channels[channel].inputSource = src;
#if LOG_LEVEL >= 1
    Serial.printf("[%lu] API -> ch%d: input %s -> %s\n",
                  millis(), channel, _inputSourceName(old), _inputSourceName(src));
#endif
}

void OutputController::setChannelPot(uint8_t channel, uint8_t potIdx) {
    if (channel >= NUM_OUTPUTS) return;
    _channels[channel].potIndex = potIdx;
}

void OutputController::setSmoothingSteps(uint8_t steps) {
    _smoothSteps = steps;
}

// --- Curve playback ---

void OutputController::playCurve(uint8_t channel, const CurvePoint* points, uint16_t count, float duration, bool loop) {
    if (channel >= NUM_OUTPUTS || count < 2) return;
    auto& ch = _channels[channel];
    // Free existing curve data
    if (ch.curveData) { free(ch.curveData); ch.curveData = nullptr; }
    // Allocate and copy
    ch.curveData = (CurvePoint*)malloc(count * sizeof(CurvePoint));
    if (!ch.curveData) { ch.curveLen = 0; return; }
    memcpy(ch.curveData, points, count * sizeof(CurvePoint));
    ch.curveLen = count;
    ch.curveDuration = duration;
    ch.curveLoop = loop;
    ch.curvePlaying = true;
    ch.curveStartMs = millis();
#if LOG_LEVEL >= 1
    Serial.printf("[Curve] ch%d: play %d pts, %.1fs, loop=%d\n", channel, count, duration, loop);
#endif
}

void OutputController::stopCurve(uint8_t channel) {
    if (channel >= NUM_OUTPUTS) return;
    _channels[channel].curvePlaying = false;
#if LOG_LEVEL >= 1
    Serial.printf("[Curve] ch%d: stop\n", channel);
#endif
}

float OutputController::_interpolateCurve(uint8_t channel, float t) const {
    const auto& ch = _channels[channel];
    if (ch.curveLen == 0) return ch.currentValue;
    if (ch.curveLen == 1) return ch.curveData[0].a;
    if (t <= ch.curveData[0].t) return ch.curveData[0].a;
    if (t >= ch.curveData[ch.curveLen - 1].t) return ch.curveData[ch.curveLen - 1].a;
    for (uint16_t i = 0; i < ch.curveLen - 1; i++) {
        if (t >= ch.curveData[i].t && t <= ch.curveData[i + 1].t) {
            float span = ch.curveData[i + 1].t - ch.curveData[i].t;
            if (span < 0.0001f) return ch.curveData[i].a;
            float frac = (t - ch.curveData[i].t) / span;
            return ch.curveData[i].a + frac * (ch.curveData[i + 1].a - ch.curveData[i].a);
        }
    }
    return ch.curveData[ch.curveLen - 1].a;
}

void OutputController::update() {
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        // Curve playback (envelope/sequence on ESP side)
        if (_channels[i].curvePlaying && _channels[i].curveData && _channels[i].curveLen >= 2) {
            float elapsed = (millis() - _channels[i].curveStartMs) / 1000.0f;
            if (_channels[i].curveLoop) {
                elapsed = fmodf(elapsed, _channels[i].curveDuration);
            } else if (elapsed > _channels[i].curveDuration) {
                _channels[i].curvePlaying = false;
                continue;
            }
            float val = _interpolateCurve(i, elapsed);
            _channels[i].currentValue = val;
            _writePWM(i, val);
            continue;
        }

        // Smoothing (ramp from current to target)
        if (_smoothSteps > 0 && fabsf(_channels[i].currentValue - _channels[i].targetValue) > 0.01f) {
            float diff = _channels[i].targetValue - _channels[i].currentValue;
            float step = diff / (float)_smoothSteps;
            if (fabsf(step) < 0.05f) step = (diff > 0) ? 0.05f : -0.05f;
            _channels[i].currentValue += step;
            if (fabsf(_channels[i].currentValue - _channels[i].targetValue) < 0.05f) {
                _channels[i].currentValue = _channels[i].targetValue;
            }
            _writePWM(i, _channels[i].currentValue);
        }
    }
}

void OutputController::getPresetJson(String& out) const {
    JsonDocument doc;
    JsonArray arr = doc["channels"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        JsonObject obj = arr.add<JsonObject>();
        const OutputChannel& ch = _channels[i];
        switch (ch.type) {
            case OUTPUT_SERVO:    obj["type"] = "servo";   break;
            case OUTPUT_MOTOR:    obj["type"] = "motor";   break;
            case OUTPUT_LEGO:     obj["type"] = "lego";    break;
            case OUTPUT_PWM:      obj["type"] = "pwm";     break;
            case OUTPUT_MOTOR_1K: obj["type"] = "motor1k"; break;
        }
        obj["input"] = _inputSourceName(ch.inputSource);
        obj["value"] = (int)roundf(ch.currentValue);
        obj["name"] = ch.name;
        obj["min"] = ch.minUs;
        obj["max"] = ch.maxUs;
        obj["pot"] = ch.potIndex;
    }
    serializeJson(doc, out);
}

bool OutputController::applyPresetJson(const uint8_t* data, size_t len) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len) != DeserializationError::Ok) return false;
    JsonArray arr = doc["channels"];
    if (arr.isNull()) return false;

    uint8_t i = 0;
    for (JsonObject obj : arr) {
        if (i >= NUM_OUTPUTS) break;

        // Type
        const char* typeStr = obj["type"] | "servo";
        if (strcmp(typeStr, "servo") == 0) setChannelType(i, OUTPUT_SERVO);
        else if (strcmp(typeStr, "motor") == 0) setChannelType(i, OUTPUT_MOTOR);
        else if (strcmp(typeStr, "lego") == 0) setChannelType(i, OUTPUT_LEGO);
        else if (strcmp(typeStr, "pwm") == 0) setChannelType(i, OUTPUT_PWM);
        else if (strcmp(typeStr, "motor1k") == 0) setChannelType(i, OUTPUT_MOTOR_1K);

        // Input source
        const char* inStr = obj["input"] | "manual";
        if (strcmp(inStr, "manual") == 0) setChannelInput(i, INPUT_MANUAL);
        else if (strcmp(inStr, "envelope") == 0) setChannelInput(i, INPUT_ENVELOPE);
        else if (strcmp(inStr, "pot") == 0) setChannelInput(i, INPUT_POT);
        else if (strcmp(inStr, "ps4_lx") == 0) setChannelInput(i, INPUT_PS4_LX);
        else if (strcmp(inStr, "ps4_ly") == 0) setChannelInput(i, INPUT_PS4_LY);
        else if (strcmp(inStr, "ps4_rx") == 0) setChannelInput(i, INPUT_PS4_RX);
        else if (strcmp(inStr, "ps4_ry") == 0) setChannelInput(i, INPUT_PS4_RY);
        else if (strcmp(inStr, "ps4_l2") == 0) setChannelInput(i, INPUT_PS4_L2);
        else if (strcmp(inStr, "ps4_r2") == 0) setChannelInput(i, INPUT_PS4_R2);
        else if (strcmp(inStr, "ps4_cross") == 0) setChannelInput(i, INPUT_PS4_CROSS);
        else if (strcmp(inStr, "ps4_circle") == 0) setChannelInput(i, INPUT_PS4_CIRCLE);
        else if (strcmp(inStr, "ps4_square") == 0) setChannelInput(i, INPUT_PS4_SQUARE);
        else if (strcmp(inStr, "ps4_tri") == 0) setChannelInput(i, INPUT_PS4_TRIANGLE);
        else if (strcmp(inStr, "ps4_l1") == 0) setChannelInput(i, INPUT_PS4_L1);
        else if (strcmp(inStr, "ps4_r1") == 0) setChannelInput(i, INPUT_PS4_R1);
        else if (strcmp(inStr, "sequence") == 0) setChannelInput(i, INPUT_SEQUENCE);

        // Value, name, range, pot
        setValue(i, (float)(obj["value"] | 90));
        if (obj["name"]) setChannelName(i, obj["name"]);
        if (obj["min"].is<uint16_t>() && obj["max"].is<uint16_t>()) {
            setChannelRange(i, obj["min"] | OUTPUT_DEFAULT_MIN_US, obj["max"] | OUTPUT_DEFAULT_MAX_US);
        }
        setChannelPot(i, obj["pot"] | 0);
        i++;
    }
    return true;
}

void OutputController::_writePWM(uint8_t channel, float value) {
    uint16_t ticks;

    switch (_channels[channel].type) {
        case OUTPUT_SERVO: {
            float us = _servoValueToPulse(channel, value);
            float periodUs = 1000000.0f / _currentFreqHz;
            ticks = (uint16_t)(us * 4096.0f / periodUs);
            break;
        }
        case OUTPUT_MOTOR:
        case OUTPUT_LEGO:
        case OUTPUT_PWM:
        case OUTPUT_MOTOR_1K: {
            ticks = (uint16_t)(value * 4095.0f / 180.0f);
            break;
        }
    }

    _pca.setPWM(channel, 0, ticks);
}

void OutputController::_updateFrequency() {
    bool hasServo = false;
    bool has1k = false;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        if (_channels[i].type == OUTPUT_SERVO) hasServo = true;
        if (_channels[i].type == OUTPUT_MOTOR_1K) has1k = true;
    }

    uint16_t targetFreq = (has1k && !hasServo) ? 1000 : OUTPUT_FREQ_HZ;

    if (targetFreq != _currentFreqHz) {
        _currentFreqHz = targetFreq;
        _pca.setPWMFreq(_currentFreqHz);
        Serial.printf("[Output] PWM frequency changed to %d Hz\n", _currentFreqHz);
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
            _writePWM(i, _channels[i].currentValue);
        }
    }
}

float OutputController::_servoValueToPulse(uint8_t channel, float value) const {
    return _channels[channel].minUs + (value / 180.0f) * (_channels[channel].maxUs - _channels[channel].minUs);
}
