#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class OutputController;
class SequenceRecorder;

class WiFiController {
public:
    void init(OutputController* outputCtrl, SequenceRecorder* seqRec);

private:
    AsyncWebServer _server{WEB_SERVER_PORT};
    OutputController* _outputCtrl = nullptr;
    SequenceRecorder* _seqRec = nullptr;

    void _setupWiFi();
    void _setupRoutes();
};
