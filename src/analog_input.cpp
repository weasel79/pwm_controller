#include "analog_input.h"
#include "output_controller.h"

void AnalogInput::init(OutputController* outputCtrl) {
    _outputCtrl = outputCtrl;
    analogReadResolution(ADC_RESOLUTION);

    for (uint8_t i = 0; i < NUM_POTS; i++) {
        pinMode(POT_PINS[i], INPUT);
        _smoothed[i] = analogRead(POT_PINS[i]);
        _lastRaw[i] = (int)_smoothed[i];
    }
    Serial.printf("[Analog] %d pots initialized\n", NUM_POTS);
}

void AnalogInput::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ANALOG_UPDATE_MS) return;
    _lastUpdateMs = now;

    if (!_outputCtrl) return;

    // For each output channel with input=pot, read the assigned pot
    for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) {
        const OutputChannel& oc = _outputCtrl->getChannel(ch);
        if (oc.inputSource != INPUT_POT) continue;
        if (oc.potIndex >= NUM_POTS) continue;

        uint8_t pi = oc.potIndex;
        int raw = _readSmoothed(pi);
        int diff = abs(raw - _lastRaw[pi]);

        if (diff > ANALOG_DEADZONE) {
            _lastRaw[pi] = raw;
            uint8_t angle = _adcToAngle((int)_smoothed[pi]);
            _outputCtrl->setValue(ch, angle);
        }
    }
}

int AnalogInput::_readSmoothed(uint8_t potIndex) {
    int raw = analogRead(POT_PINS[potIndex]);
    _smoothed[potIndex] += ANALOG_SMOOTHING * (raw - _smoothed[potIndex]);
    return raw;
}

uint8_t AnalogInput::_adcToAngle(int value) const {
    int maxAdc = (1 << ADC_RESOLUTION) - 1;
    return (uint8_t)constrain(map(value, 0, maxAdc, 0, 180), 0, 180);
}
