#pragma once

#include <Arduino.h>
#include "config.h"

class OutputController;

// Abstract pin reader for future mux support (e.g. CD74HC4067)
class PinReader {
public:
    virtual ~PinReader() {}
    virtual bool readPin(uint8_t index) = 0;
};

// Direct GPIO pin reader using POT_PINS[]
class GpioPinReader : public PinReader {
public:
    void init();
    bool readPin(uint8_t index) override;
};

class DigitalInput {
public:
    void init(OutputController* outputCtrl, PinReader* reader = nullptr);
    void update(); // Call in loop()
    bool getPinState(uint8_t index) const;

private:
    OutputController* _outputCtrl = nullptr;
    PinReader* _reader = nullptr;
    GpioPinReader _defaultReader;

    static const uint8_t MAX_PINS = NUM_POTS;
    bool _pinState[MAX_PINS] = {};       // Current debounced state
    bool _lastRaw[MAX_PINS] = {};        // Last raw reading
    unsigned long _lastChange[MAX_PINS] = {}; // Debounce timestamp
    bool _toggleState[MAX_PINS] = {};    // Toggle flip-flop
    unsigned long _lastUpdateMs = 0;

    static const unsigned long DEBOUNCE_MS = 50;
    static const unsigned long UPDATE_MS = 10;
};
