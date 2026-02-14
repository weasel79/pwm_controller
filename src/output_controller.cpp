#include "output_controller.h"
#include <Wire.h>

static const char* _inputSourceName(InputSource s) {
    switch (s) {
        case INPUT_MANUAL: return "manual"; case INPUT_ENVELOPE: return "envelope";
        case INPUT_POT: return "pot"; case INPUT_PS4_LX: return "ps4_lx";
        case INPUT_PS4_LY: return "ps4_ly"; case INPUT_PS4_RX: return "ps4_rx";
        case INPUT_PS4_RY: return "ps4_ry"; case INPUT_PS4_L2: return "ps4_l2";
        case INPUT_PS4_R2: return "ps4_r2"; case INPUT_PS4_CROSS: return "ps4_cross";
        case INPUT_PS4_CIRCLE: return "ps4_circle"; case INPUT_PS4_SQUARE: return "ps4_square";
        case INPUT_PS4_TRIANGLE: return "ps4_tri"; case INPUT_PS4_L1: return "ps4_l1";
        case INPUT_PS4_R1: return "ps4_r1"; default: return "?";
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
        _channels[i].currentValue = OUTPUT_DEFAULT_VALUE;
        _channels[i].targetValue = OUTPUT_DEFAULT_VALUE;
        _channels[i].enabled = true;
        _channels[i].inputSource = INPUT_MANUAL;
        snprintf(_channels[i].name, sizeof(_channels[i].name), "Output %d", i);
    }

    // Move all outputs to default position
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        _writePWM(i, OUTPUT_DEFAULT_VALUE);
    }

    Serial.println("[Output] PCA9685 initialized, all outputs at default value");
}

void OutputController::setValue(uint8_t channel, uint8_t value) {
    if (channel >= NUM_OUTPUTS) return;
    value = constrain(value, 0, 180);

    _channels[channel].targetValue = value;

    if (_smoothSteps == 0) {
        _channels[channel].currentValue = value;
        _writePWM(channel, value);
    }

#if LOG_LEVEL >= 2
    static unsigned long _lastTraceMs[NUM_OUTPUTS] = {};
    unsigned long now = millis();
    uint8_t oldVal = _channels[channel].currentValue;
    if (oldVal != value && (now - _lastTraceMs[channel] >= 1000)) {
        _lastTraceMs[channel] = now;
        Serial.printf("[%lu] SET -> ch%d: %d -> %d (src=%s)\n",
                      now, channel, oldVal, value,
                      _inputSourceName(_channels[channel].inputSource));
    }
#endif
}

uint8_t OutputController::getValue(uint8_t channel) const {
    if (channel >= NUM_OUTPUTS) return 0;
    return _channels[channel].currentValue;
}

void OutputController::setAllValues(const uint8_t values[], uint8_t count) {
    uint8_t n = min((uint8_t)NUM_OUTPUTS, count);
    for (uint8_t i = 0; i < n; i++) {
        setValue(i, values[i]);
    }
}

void OutputController::getAllValues(uint8_t outValues[], uint8_t count) const {
    uint8_t n = min((uint8_t)NUM_OUTPUTS, count);
    for (uint8_t i = 0; i < n; i++) {
        outValues[i] = _channels[i].currentValue;
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

void OutputController::setSmoothingSteps(uint8_t steps) {
    _smoothSteps = steps;
}

void OutputController::update() {
    if (_smoothSteps == 0) return;

    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        if (_channels[i].currentValue != _channels[i].targetValue) {
            int16_t diff = (int16_t)_channels[i].targetValue - (int16_t)_channels[i].currentValue;
            int16_t step = diff / (int16_t)_smoothSteps;
            if (step == 0) step = (diff > 0) ? 1 : -1;

            _channels[i].currentValue = (uint8_t)((int16_t)_channels[i].currentValue + step);
            _writePWM(i, _channels[i].currentValue);
        }
    }
}

void OutputController::_writePWM(uint8_t channel, uint8_t value) {
    uint16_t ticks;

    switch (_channels[channel].type) {
        case OUTPUT_SERVO: {
            // Servo: map value (0-180) to pulse width, then to ticks
            uint16_t us = _servoValueToPulse(channel, value);
            uint32_t periodUs = 1000000UL / _currentFreqHz;
            ticks = (uint16_t)((uint32_t)us * 4096 / periodUs);
            break;
        }
        case OUTPUT_MOTOR:
        case OUTPUT_LEGO:
        case OUTPUT_PWM:
        case OUTPUT_MOTOR_1K: {
            // Motor/Lego/PWM: map value (0-180) to full duty cycle (0-4095)
            ticks = (uint16_t)map(value, 0, 180, 0, 4095);
            break;
        }
    }

    _pca.setPWM(channel, 0, ticks);
}

void OutputController::_updateFrequency() {
    // PCA9685 has one frequency for all 16 channels.
    // Use 1000Hz if any channel needs it and no servo channels exist.
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
        // Re-apply all current values with new frequency
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
            _writePWM(i, _channels[i].currentValue);
        }
    }
}

uint16_t OutputController::_servoValueToPulse(uint8_t channel, uint8_t value) const {
    return map(value, 0, 180, _channels[channel].minUs, _channels[channel].maxUs);
}
