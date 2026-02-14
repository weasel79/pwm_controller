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
};
