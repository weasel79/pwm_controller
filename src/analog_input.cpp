#include "analog_input.h"
#include "servo_controller.h"

void AnalogInput::init(ServoController* servoCtrl) {
    _servoCtrl = servoCtrl;
    _potCount = 0;
    _joyCount = 0;
    analogReadResolution(ADC_RESOLUTION);
    Serial.println("[Analog] Input initialized");
}

void AnalogInput::addPot(uint8_t adcPin, uint8_t servoChannel) {
    if (_potCount >= MAX_POTS) return;
    PotMapping& p = _pots[_potCount++];
    p.adcPin = adcPin;
    p.servoChannel = servoChannel;
    p.smoothedValue = analogRead(adcPin);
    p.lastRawValue = (int)p.smoothedValue;
    p.active = false;
    pinMode(adcPin, INPUT);
    Serial.printf("[Analog] Pot on GPIO %d -> Servo %d\n", adcPin, servoChannel);
}

void AnalogInput::addJoystick(uint8_t xPin, uint8_t yPin, uint8_t servoChX, uint8_t servoChY) {
    if (_joyCount >= MAX_JOYSTICKS) return;
    JoystickMapping& j = _joys[_joyCount++];
    j.xPin = xPin;
    j.yPin = yPin;
    j.servoChannelX = servoChX;
    j.servoChannelY = servoChY;
    j.smoothedX = analogRead(xPin);
    j.smoothedY = analogRead(yPin);
    j.lastRawX = (int)j.smoothedX;
    j.lastRawY = (int)j.smoothedY;
    pinMode(xPin, INPUT);
    pinMode(yPin, INPUT);
    Serial.printf("[Analog] Joystick X:GPIO%d->Servo%d, Y:GPIO%d->Servo%d\n",
                  xPin, servoChX, yPin, servoChY);
}

void AnalogInput::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ANALOG_UPDATE_MS) return;
    _lastUpdateMs = now;

    if (!_servoCtrl) return;

    // Read potentiometers
    for (uint8_t i = 0; i < _potCount; i++) {
        PotMapping& p = _pots[i];
        int raw = _readSmoothed(p.adcPin, p.smoothedValue, p.lastRawValue);
        int diff = abs(raw - p.lastRawValue);

        if (diff > ANALOG_DEADZONE) {
            p.active = true;
            p.lastRawValue = raw;
            uint8_t angle = _adcToAngle((int)p.smoothedValue);
            _servoCtrl->setAngle(p.servoChannel, angle);
        } else {
            p.active = false;
        }
    }

    // Read joysticks
    for (uint8_t i = 0; i < _joyCount; i++) {
        JoystickMapping& j = _joys[i];

        int rawX = _readSmoothed(j.xPin, j.smoothedX, j.lastRawX);
        int rawY = _readSmoothed(j.yPin, j.smoothedY, j.lastRawY);

        if (abs(rawX - j.lastRawX) > ANALOG_DEADZONE) {
            j.lastRawX = rawX;
            _servoCtrl->setAngle(j.servoChannelX, _adcToAngle((int)j.smoothedX));
        }
        if (abs(rawY - j.lastRawY) > ANALOG_DEADZONE) {
            j.lastRawY = rawY;
            _servoCtrl->setAngle(j.servoChannelY, _adcToAngle((int)j.smoothedY));
        }
    }
}

bool AnalogInput::isChannelActive(uint8_t servoChannel) const {
    for (uint8_t i = 0; i < _potCount; i++) {
        if (_pots[i].servoChannel == servoChannel && _pots[i].active) return true;
    }
    return false;
}

int AnalogInput::_readSmoothed(uint8_t pin, float& smoothed, int lastRaw) {
    int raw = analogRead(pin);
    smoothed = smoothed + ANALOG_SMOOTHING * (raw - smoothed);
    return raw;
}

uint8_t AnalogInput::_adcToAngle(int value) const {
    int maxAdc = (1 << ADC_RESOLUTION) - 1;
    return (uint8_t)constrain(map(value, 0, maxAdc, 0, 180), 0, 180);
}
