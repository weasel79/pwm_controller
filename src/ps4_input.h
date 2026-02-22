#pragma once

#include <Arduino.h>
#include "config.h"

class OutputController;

// Cached PS4 state — safe to read from any task
struct PS4State {
    bool connected = false;
    int8_t lx = 0, ly = 0, rx = 0, ry = 0;
    uint8_t l2 = 0, r2 = 0;
    bool cross = false, circle = false, square = false, triangle = false;
    bool l1 = false, r1 = false;
};

class PS4Input {
public:
    void init(OutputController* outputCtrl);
    void update(); // Call in loop()
    bool isConnected() const { return _connected; }
    const PS4State& getState() const { return _state; }

private:
    OutputController* _outputCtrl = nullptr;
    bool _connected = false;
    bool _wasConnected = false;
    unsigned long _lastUpdateMs = 0;
    PS4State _state; // Cached values, updated only in main loop
#if LOG_LEVEL >= 2
    unsigned long _lastTraceMs = 0;
#endif
};
