#pragma once

// MouldKing BLE motor controller — 6 channels (A-F).
// Uses Bluedroid BLE GAP advertising (compatible with PS4 Classic BT).
// MK 6.0 protocol from MouldKingino (MIT license, github.com/pink0D/MouldKingino).
// Values 0-180 map to speed -1.0..+1.0 where 90 = stopped.
// Supports curve playback (envelope/sequence) per channel.

#include <Arduino.h>
#include "config.h"
#include "output_controller.h"  // for InputSource, CurvePoint

class PS4Input;

class MkInput {
public:
    void init(PS4Input* ps4 = nullptr);
    void connect();
    void disconnect();
    void update();

    // Motor value for channel 0-5, range 0-180 (90=stop)
    void setValue(uint8_t channel, float value);
    float getValue(uint8_t channel) const;

    // Input source per channel
    void setInputSource(uint8_t channel, InputSource src);
    InputSource getInputSource(uint8_t channel) const;

    // Pot assignment per channel
    void setPotIndex(uint8_t channel, uint8_t potIdx);
    uint8_t getPotIndex(uint8_t channel) const;

    // Curve playback (envelope/sequence)
    void playCurve(uint8_t channel, const CurvePoint* points, uint16_t count, float duration, bool loop);
    void stopCurve(uint8_t channel);
    void stopAllCurves();

    bool isConnected() const { return _connected; }

private:
    bool _connected = false;
    float _values[NUM_MK_CHANNELS];
    InputSource _inputSrc[NUM_MK_CHANNELS];
    uint8_t _potIndex[NUM_MK_CHANNELS];
    PS4Input* _ps4 = nullptr;
    unsigned long _lastUpdateMs = 0;

    // Curve playback per channel
    CurvePoint* _curveData[NUM_MK_CHANNELS];
    uint16_t    _curveLen[NUM_MK_CHANNELS];
    float       _curveDur[NUM_MK_CHANNELS];
    bool        _curveLoop[NUM_MK_CHANNELS];
    bool        _curvePlaying[NUM_MK_CHANNELS];
    unsigned long _curveStartMs[NUM_MK_CHANNELS];

    float _interpolateCurve(uint8_t ch, float t) const;
};
