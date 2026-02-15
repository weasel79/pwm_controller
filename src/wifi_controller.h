#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class OutputController;

class WiFiController {
public:
    void init(OutputController* outputCtrl);

private:
    AsyncWebServer _server{WEB_SERVER_PORT};
    OutputController* _outputCtrl = nullptr;

    void _setupWiFi();
    void _setupRoutes();
};
