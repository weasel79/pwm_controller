#include "digital_input.h"
#include "output_controller.h"

void GpioPinReader::init() {
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        // GPIO 34-35 have no internal pullup; GPIO 32-33 do
        if (POT_PINS[i] >= 34) {
            pinMode(POT_PINS[i], INPUT);
        } else {
            pinMode(POT_PINS[i], INPUT_PULLUP);
        }
    }
}

bool GpioPinReader::readPin(uint8_t index) {
    if (index >= NUM_POTS) return false;
    return digitalRead(POT_PINS[index]) == HIGH;
}

void DigitalInput::init(OutputController* outputCtrl, PinReader* reader) {
    _outputCtrl = outputCtrl;
    if (reader) {
        _reader = reader;
    } else {
        _defaultReader.init();
        _reader = &_defaultReader;
    }
    for (uint8_t i = 0; i < MAX_PINS; i++) {
        _pinState[i] = false;
        _lastRaw[i] = false;
        _lastChange[i] = 0;
        _toggleState[i] = false;
    }
}

bool DigitalInput::getPinState(uint8_t index) const {
    if (index >= MAX_PINS) return false;
    return _pinState[index];
}

void DigitalInput::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < UPDATE_MS) return;
    _lastUpdateMs = now;

    // Build set of pins actually used by digital-input channels
    bool pinUsed[MAX_PINS] = {};
    if (_outputCtrl) {
        for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) {
            const auto& oc = _outputCtrl->getChannel(ch);
            if (oc.inputSource == INPUT_DIGITAL && oc.digitalPin < MAX_PINS)
                pinUsed[oc.digitalPin] = true;
        }
    }

    // Debounce only pins that are assigned to a digital-input channel
    for (uint8_t i = 0; i < MAX_PINS; i++) {
        if (!pinUsed[i]) continue;
        bool raw = _reader->readPin(i);
        if (raw != _lastRaw[i]) {
            _lastChange[i] = now;
            _lastRaw[i] = raw;
        }
        if (now - _lastChange[i] >= DEBOUNCE_MS) {
            bool prev = _pinState[i];
            _pinState[i] = _lastRaw[i];
            // Detect rising edge for toggle
            if (_pinState[i] && !prev) {
                _toggleState[i] = !_toggleState[i];
            }
        }
    }

    // Drive output channels that have INPUT_DIGITAL
    if (!_outputCtrl) return;
    for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) {
        const auto& oc = _outputCtrl->getChannel(ch);
        if (oc.inputSource != INPUT_DIGITAL) continue;
        uint8_t pin = oc.digitalPin;
        if (pin >= MAX_PINS) continue;

        float val;
        if (oc.digitalMode == 0) {
            // Toggle mode: flip-flop between 0 and 180 on rising edge
            val = _toggleState[pin] ? 180.0f : 0.0f;
        } else {
            // Latch mode: HIGH=180, LOW=0
            val = _pinState[pin] ? 180.0f : 0.0f;
        }
        _outputCtrl->setValue(ch, val);
    }
}
