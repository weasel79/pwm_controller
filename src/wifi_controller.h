#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class ServoController;
class SequenceRecorder;

class WiFiController {
public:
    void init(ServoController* servoCtrl, SequenceRecorder* seqRec);

private:
    AsyncWebServer _server{WEB_SERVER_PORT};
    ServoController* _servoCtrl = nullptr;
    SequenceRecorder* _seqRec = nullptr;

    void _setupWiFi();
    void _setupRoutes();
};
