#pragma once

#include <Arduino.h>
#include "config.h"

struct PotMapping {
    uint8_t adcPin;
    uint8_t outputChannel;
    float   smoothedValue;
    int     lastRawValue;
    bool    active; // True if value is currently changing
};

struct JoystickMapping {
    uint8_t xPin;
    uint8_t yPin;
    uint8_t outputChannelX;
    uint8_t outputChannelY;
    float   smoothedX;
    float   smoothedY;
    int     lastRawX;
    int     lastRawY;
};

class OutputController;

class AnalogInput {
public:
    void init(OutputController* outputCtrl);
    void addPot(uint8_t adcPin, uint8_t outputChannel);
    void addJoystick(uint8_t xPin, uint8_t yPin, uint8_t outChX, uint8_t outChY);
    void update();
    bool isChannelActive(uint8_t outputChannel) const;

private:
    OutputController* _outputCtrl = nullptr;
    static const uint8_t MAX_POTS = 8;
    static const uint8_t MAX_JOYSTICKS = 4;
    PotMapping _pots[MAX_POTS];
    uint8_t _potCount = 0;
    JoystickMapping _joys[MAX_JOYSTICKS];
    uint8_t _joyCount = 0;
    unsigned long _lastUpdateMs = 0;
    uint8_t _activeChannels = 0; // Bitmask for channels 0-7

    int _readSmoothed(uint8_t pin, float& smoothed, int lastRaw);
    uint8_t _adcToAngle(int value) const;
};
