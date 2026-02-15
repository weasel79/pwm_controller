#pragma once

#include <Arduino.h>
#include "config.h"

class OutputController;

class AnalogInput {
public:
    void init(OutputController* outputCtrl);
    void update();

private:
    OutputController* _outputCtrl = nullptr;
    float   _smoothed[NUM_POTS];
    int     _lastRaw[NUM_POTS];
    unsigned long _lastUpdateMs = 0;

    int _readSmoothed(uint8_t potIndex);
    uint8_t _adcToAngle(int value) const;
};
