#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

class OutputController;
class DigitalInput;
class PS4Input;
class MkInput;

class WiFiController {
public:
    void init(OutputController* outputCtrl, DigitalInput* digitalInput = nullptr,
              PS4Input* ps4Input = nullptr, MkInput* mkInput = nullptr);

private:
    AsyncWebServer _server{WEB_SERVER_PORT};
    OutputController* _outputCtrl = nullptr;
    DigitalInput* _digitalInput = nullptr;
    PS4Input* _ps4Input = nullptr;
    MkInput* _mkInput = nullptr;
    String _activeSSID;  // Current WiFi SSID (from /wifi.json or config.h default)

    void _setupWiFi();
    void _setupRoutes();
};
