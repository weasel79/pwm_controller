#include "servo_controller.h"
#include <Wire.h>

void ServoController::init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    _pca.begin();
    _pca.setOscillatorFrequency(25000000);
    _pca.setPWMFreq(SERVO_FREQ_HZ);

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        _channels[i].channel = i;
        _channels[i].minUs = SERVO_MIN_US;
        _channels[i].maxUs = SERVO_MAX_US;
        _channels[i].currentAngle = SERVO_DEFAULT_ANGLE;
        _channels[i].targetAngle = SERVO_DEFAULT_ANGLE;
        _channels[i].enabled = true;
        snprintf(_channels[i].name, sizeof(_channels[i].name), "Servo %d", i);
    }

    // Move all servos to default position
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        _writeMicroseconds(i, _angleToPulse(i, SERVO_DEFAULT_ANGLE));
    }

    Serial.println("[Servo] PCA9685 initialized, all servos at default angle");
}

void ServoController::setAngle(uint8_t channel, uint8_t angle) {
    if (channel >= NUM_SERVOS) return;
    angle = constrain(angle, 0, 180);

    _channels[channel].targetAngle = angle;

    if (_smoothSteps == 0) {
        _channels[channel].currentAngle = angle;
        _writeMicroseconds(channel, _angleToPulse(channel, angle));
    }
}

uint8_t ServoController::getAngle(uint8_t channel) const {
    if (channel >= NUM_SERVOS) return 0;
    return _channels[channel].currentAngle;
}

void ServoController::setAllAngles(const uint8_t angles[], uint8_t count) {
    uint8_t n = min((uint8_t)NUM_SERVOS, count);
    for (uint8_t i = 0; i < n; i++) {
        setAngle(i, angles[i]);
    }
}

void ServoController::getAllAngles(uint8_t outAngles[], uint8_t count) const {
    uint8_t n = min((uint8_t)NUM_SERVOS, count);
    for (uint8_t i = 0; i < n; i++) {
        outAngles[i] = _channels[i].currentAngle;
    }
}

const ServoChannel& ServoController::getChannel(uint8_t ch) const {
    return _channels[ch < NUM_SERVOS ? ch : 0];
}

void ServoController::setChannelRange(uint8_t channel, uint16_t minUs, uint16_t maxUs) {
    if (channel >= NUM_SERVOS) return;
    _channels[channel].minUs = minUs;
    _channels[channel].maxUs = maxUs;
}

void ServoController::setChannelName(uint8_t channel, const char* name) {
    if (channel >= NUM_SERVOS) return;
    strncpy(_channels[channel].name, name, sizeof(_channels[channel].name) - 1);
    _channels[channel].name[sizeof(_channels[channel].name) - 1] = '\0';
}

void ServoController::setSmoothingSteps(uint8_t steps) {
    _smoothSteps = steps;
}

void ServoController::update() {
    if (_smoothSteps == 0) return;

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        if (_channels[i].currentAngle != _channels[i].targetAngle) {
            int16_t diff = (int16_t)_channels[i].targetAngle - (int16_t)_channels[i].currentAngle;
            int16_t step = diff / (int16_t)_smoothSteps;
            if (step == 0) step = (diff > 0) ? 1 : -1;

            _channels[i].currentAngle = (uint8_t)((int16_t)_channels[i].currentAngle + step);
            _writeMicroseconds(i, _angleToPulse(i, _channels[i].currentAngle));
        }
    }
}

uint16_t ServoController::_angleToPulse(uint8_t channel, uint8_t angle) const {
    return map(angle, 0, 180, _channels[channel].minUs, _channels[channel].maxUs);
}

void ServoController::_writeMicroseconds(uint8_t channel, uint16_t us) {
    // PCA9685 runs at 50Hz = 20ms period, 4096 ticks per period
    // ticks = us * 4096 / 20000
    uint16_t ticks = (uint16_t)((uint32_t)us * 4096 / 20000);
    _pca.setPWM(channel, 0, ticks);
}
