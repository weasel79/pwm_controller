#include "ps4_input.h"
#include "output_controller.h"
#include <PS4Controller.h>

void PS4Input::init(OutputController* outputCtrl) {
    _outputCtrl = outputCtrl;
    PS4.begin(PS4_CONTROLLER_MAC);
    Serial.printf("[PS4] Waiting for controller, pair MAC: %s\n", PS4_CONTROLLER_MAC);
    Serial.printf("[Heap] After PS4 init: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void PS4Input::update() {
    _connected = PS4.isConnected();

    if (_connected && !_wasConnected) {
        Serial.println("[PS4] Controller connected");
        _wasConnected = true;
    } else if (!_connected && _wasConnected) {
        Serial.println("[PS4] Controller disconnected");
        _wasConnected = false;
    }

    if (!_connected || !_outputCtrl) return;

    unsigned long now = millis();
    if (now - _lastUpdateMs < PS4_UPDATE_MS) return;
    _lastUpdateMs = now;

    // Read PS4 values
    int lx = PS4.LStickX();   // -128..127
    int ly = PS4.LStickY();
    int rx = PS4.RStickX();
    int ry = PS4.RStickY();
    int l2 = PS4.L2Value();   // 0..255
    int r2 = PS4.R2Value();

    for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) {
        const OutputChannel& oc = _outputCtrl->getChannel(ch);
        uint8_t val = 0;

        switch (oc.inputSource) {
            case INPUT_PS4_LX:
                val = (uint8_t)map(lx, -128, 127, 0, 180);
                break;
            case INPUT_PS4_LY:
                val = (uint8_t)map(ly, -128, 127, 0, 180);
                break;
            case INPUT_PS4_RX:
                val = (uint8_t)map(rx, -128, 127, 0, 180);
                break;
            case INPUT_PS4_RY:
                val = (uint8_t)map(ry, -128, 127, 0, 180);
                break;
            case INPUT_PS4_L2:
                val = (uint8_t)map(l2, 0, 255, 0, 180);
                break;
            case INPUT_PS4_R2:
                val = (uint8_t)map(r2, 0, 255, 0, 180);
                break;
            case INPUT_PS4_CROSS:
                val = PS4.Cross() ? 180 : 0;
                break;
            case INPUT_PS4_CIRCLE:
                val = PS4.Circle() ? 180 : 0;
                break;
            case INPUT_PS4_SQUARE:
                val = PS4.Square() ? 180 : 0;
                break;
            case INPUT_PS4_TRIANGLE:
                val = PS4.Triangle() ? 180 : 0;
                break;
            case INPUT_PS4_L1:
                val = PS4.L1() ? 180 : 0;
                break;
            case INPUT_PS4_R1:
                val = PS4.R1() ? 180 : 0;
                break;
            default:
                continue; // Not a PS4 input source
        }

        _outputCtrl->setValue(ch, val);
    }
}
