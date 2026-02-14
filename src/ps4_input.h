#pragma once

#include <Arduino.h>
#include "config.h"

class OutputController;

class PS4Input {
public:
    void init(OutputController* outputCtrl);
    void update(); // Call in loop()
    bool isConnected() const { return _connected; }

private:
    OutputController* _outputCtrl = nullptr;
    bool _connected = false;
    bool _wasConnected = false;
    unsigned long _lastUpdateMs = 0;
#if LOG_LEVEL >= 2
    unsigned long _lastTraceMs = 0;
#endif
};
