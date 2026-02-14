#include "analog_input.h"
#include "output_controller.h"

void AnalogInput::init(OutputController* outputCtrl) {
    _outputCtrl = outputCtrl;
    _potCount = 0;
    _joyCount = 0;
    analogReadResolution(ADC_RESOLUTION);
    Serial.println("[Analog] Input initialized");
}

void AnalogInput::addPot(uint8_t adcPin, uint8_t outputChannel) {
    if (_potCount >= MAX_POTS) return;
    PotMapping& p = _pots[_potCount++];
    p.adcPin = adcPin;
    p.outputChannel = outputChannel;
    p.smoothedValue = analogRead(adcPin);
    p.lastRawValue = (int)p.smoothedValue;
    p.active = false;
    pinMode(adcPin, INPUT);
    Serial.printf("[Analog] Pot on GPIO %d -> Output %d\n", adcPin, outputChannel);
}

void AnalogInput::addJoystick(uint8_t xPin, uint8_t yPin, uint8_t outChX, uint8_t outChY) {
    if (_joyCount >= MAX_JOYSTICKS) return;
    JoystickMapping& j = _joys[_joyCount++];
    j.xPin = xPin;
    j.yPin = yPin;
    j.outputChannelX = outChX;
    j.outputChannelY = outChY;
    j.smoothedX = analogRead(xPin);
    j.smoothedY = analogRead(yPin);
    j.lastRawX = (int)j.smoothedX;
    j.lastRawY = (int)j.smoothedY;
    pinMode(xPin, INPUT);
    pinMode(yPin, INPUT);
    Serial.printf("[Analog] Joystick X:GPIO%d->Output%d, Y:GPIO%d->Output%d\n",
                  xPin, outChX, yPin, outChY);
}

void AnalogInput::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ANALOG_UPDATE_MS) return;
    _lastUpdateMs = now;

    if (!_outputCtrl) return;

    // Read potentiometers
    for (uint8_t i = 0; i < _potCount; i++) {
        PotMapping& p = _pots[i];
        // Only drive channels with input source set to POT
        if (_outputCtrl->getChannel(p.outputChannel).inputSource != INPUT_POT) {
            p.active = false;
            continue;
        }
        int raw = _readSmoothed(p.adcPin, p.smoothedValue, p.lastRawValue);
        int diff = abs(raw - p.lastRawValue);

        if (diff > ANALOG_DEADZONE) {
            p.active = true;
            p.lastRawValue = raw;
            uint8_t angle = _adcToAngle((int)p.smoothedValue);
            _outputCtrl->setValue(p.outputChannel, angle);
        } else {
            p.active = false;
        }
    }

    // Read joysticks
    for (uint8_t i = 0; i < _joyCount; i++) {
        JoystickMapping& j = _joys[i];

        int rawX = _readSmoothed(j.xPin, j.smoothedX, j.lastRawX);
        int rawY = _readSmoothed(j.yPin, j.smoothedY, j.lastRawY);

        if (_outputCtrl->getChannel(j.outputChannelX).inputSource == INPUT_POT &&
            abs(rawX - j.lastRawX) > ANALOG_DEADZONE) {
            j.lastRawX = rawX;
            _outputCtrl->setValue(j.outputChannelX, _adcToAngle((int)j.smoothedX));
        }
        if (_outputCtrl->getChannel(j.outputChannelY).inputSource == INPUT_POT &&
            abs(rawY - j.lastRawY) > ANALOG_DEADZONE) {
            j.lastRawY = rawY;
            _outputCtrl->setValue(j.outputChannelY, _adcToAngle((int)j.smoothedY));
        }
    }
}

bool AnalogInput::isChannelActive(uint8_t outputChannel) const {
    for (uint8_t i = 0; i < _potCount; i++) {
        if (_pots[i].outputChannel == outputChannel && _pots[i].active) return true;
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
