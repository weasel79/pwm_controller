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

    // Always cache state (defaults to 0/false when disconnected)
    _state.connected = _connected;
    if (_connected) {
        _state.lx = PS4.LStickX();
        _state.ly = PS4.LStickY();
        _state.rx = PS4.RStickX();
        _state.ry = PS4.RStickY();
        _state.l2 = PS4.L2Value();
        _state.r2 = PS4.R2Value();
        _state.cross = PS4.Cross();
        _state.circle = PS4.Circle();
        _state.square = PS4.Square();
        _state.triangle = PS4.Triangle();
        _state.l1 = PS4.L1();
        _state.r1 = PS4.R1();
    }

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

    // Use cached values
    int lx = _state.lx;
    int ly = _state.ly;
    int rx = _state.rx;
    int ry = _state.ry;
    int l2 = _state.l2;
    int r2 = _state.r2;

#if LOG_LEVEL >= 2
    bool doTrace = (now - _lastTraceMs >= 1000);
    if (doTrace) {
        _lastTraceMs = now;
        Serial.printf("[%lu] PS4: LX=%d LY=%d RX=%d RY=%d L2=%d R2=%d\n",
                      now, lx, ly, rx, ry, l2, r2);
    }
#else
    bool doTrace = false;
#endif

    for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) {
        const OutputChannel& oc = _outputCtrl->getChannel(ch);
        uint8_t val = 0;
        const char* srcName = nullptr;

        switch (oc.inputSource) {
            case INPUT_PS4_LX:
                val = (uint8_t)map(lx, -128, 127, 0, 180);
                srcName = "LX";
                break;
            case INPUT_PS4_LY:
                val = (uint8_t)map(ly, -128, 127, 0, 180);
                srcName = "LY";
                break;
            case INPUT_PS4_RX:
                val = (uint8_t)map(rx, -128, 127, 0, 180);
                srcName = "RX";
                break;
            case INPUT_PS4_RY:
                val = (uint8_t)map(ry, -128, 127, 0, 180);
                srcName = "RY";
                break;
            case INPUT_PS4_L2:
                val = (uint8_t)map(l2, 0, 255, 0, 180);
                srcName = "L2";
                break;
            case INPUT_PS4_R2:
                val = (uint8_t)map(r2, 0, 255, 0, 180);
                srcName = "R2";
                break;
            case INPUT_PS4_CROSS:
                val = _state.cross ? 180 : 0;
                srcName = "CROSS";
                break;
            case INPUT_PS4_CIRCLE:
                val = _state.circle ? 180 : 0;
                srcName = "CIRCLE";
                break;
            case INPUT_PS4_SQUARE:
                val = _state.square ? 180 : 0;
                srcName = "SQUARE";
                break;
            case INPUT_PS4_TRIANGLE:
                val = _state.triangle ? 180 : 0;
                srcName = "TRI";
                break;
            case INPUT_PS4_L1:
                val = _state.l1 ? 180 : 0;
                srcName = "L1";
                break;
            case INPUT_PS4_R1:
                val = _state.r1 ? 180 : 0;
                srcName = "R1";
                break;
            default:
                continue; // Not a PS4 input source
        }

#if LOG_LEVEL >= 2
        if (doTrace) {
            Serial.printf("[%lu] PS4 %s -> ch%d: val=%d\n", now, srcName, ch, val);
        }
#endif
        _outputCtrl->setValue(ch, val);
    }
}
