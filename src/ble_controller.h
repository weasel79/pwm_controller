#pragma once

#include <Arduino.h>
#include "config.h"

class ServoController;

class BLEController {
public:
    void init(ServoController* servoCtrl);
    void updateStateNotification();

private:
    ServoController* _servoCtrl = nullptr;
    bool _initialized = false;
};
