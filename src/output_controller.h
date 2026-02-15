#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

enum InputSource : uint8_t {
    INPUT_MANUAL     = 0,
    INPUT_ENVELOPE   = 1,
    INPUT_POT        = 2,
    INPUT_PS4_LX     = 3,
    INPUT_PS4_LY     = 4,
    INPUT_PS4_RX     = 5,
    INPUT_PS4_RY     = 6,
    INPUT_PS4_L2     = 7,
    INPUT_PS4_R2     = 8,
    INPUT_PS4_CROSS  = 9,
    INPUT_PS4_CIRCLE = 10,
    INPUT_PS4_SQUARE = 11,
    INPUT_PS4_TRIANGLE = 12,
    INPUT_PS4_L1     = 13,
    INPUT_PS4_R1     = 14,
    INPUT_SEQUENCE   = 15,
};

enum OutputType : uint8_t {
    OUTPUT_SERVO = 0,   // Standard servo: 500-2500us pulse, 0-180 = angle
    OUTPUT_MOTOR = 1,   // DC motor: full PWM duty cycle, 0-180 = speed %
    OUTPUT_LEGO  = 2,   // Lego motor: full PWM duty cycle, 0-180 = speed %
    OUTPUT_PWM      = 3,   // Raw PWM: full duty cycle, 0-180 = 0-100%
    OUTPUT_MOTOR_1K = 4    // DC motor @ 1kHz: full duty cycle, 0-180 = speed %
};

struct CurvePoint {
    float t;  // time in seconds
    float a;  // angle 0-180 (float precision)
};

struct OutputChannel {
    uint8_t    channel;
    OutputType type;
    uint16_t   minUs;
    uint16_t   maxUs;
    float      currentValue;
    float      targetValue;
    char       name[16];
    bool       enabled;
    InputSource inputSource;
    uint8_t    potIndex;       // Which physical pot (0..NUM_POTS-1) when input=pot
    // Curve playback (envelope / sequence)
    CurvePoint* curveData = nullptr;
    uint16_t   curveLen = 0;
    float      curveDuration = 0;
    bool       curveLoop = false;
    bool       curvePlaying = false;
    unsigned long curveStartMs = 0;
};

class OutputController {
public:
    void init();
    void setValue(uint8_t channel, float value);
    uint8_t getValue(uint8_t channel) const;
    void setAllValues(const uint8_t values[], uint8_t count);
    void getAllValues(uint8_t outValues[], uint8_t count) const;
    const OutputChannel& getChannel(uint8_t ch) const;
    void setChannelRange(uint8_t channel, uint16_t minUs, uint16_t maxUs);
    void setChannelName(uint8_t channel, const char* name);
    void setChannelType(uint8_t channel, OutputType type);
    void setChannelInput(uint8_t channel, InputSource src);
    void setChannelPot(uint8_t channel, uint8_t potIndex);
    void setSmoothingSteps(uint8_t steps);
    void update(); // Call in loop()

    // Curve playback (used for envelope and sequence on ESP side)
    void playCurve(uint8_t channel, const CurvePoint* points, uint16_t count, float duration, bool loop);
    void stopCurve(uint8_t channel);

    // Preset save/load
    void getPresetJson(String& out) const;
    bool applyPresetJson(const uint8_t* data, size_t len);

private:
    Adafruit_PWMServoDriver _pca = Adafruit_PWMServoDriver(PCA9685_ADDR);
    OutputChannel _channels[NUM_OUTPUTS];
    uint8_t _smoothSteps = 0; // 0 = instant
    uint16_t _currentFreqHz = OUTPUT_FREQ_HZ;
    void _writePWM(uint8_t channel, float value);
    void _updateFrequency();
    float _servoValueToPulse(uint8_t channel, float value) const;
    float _interpolateCurve(uint8_t channel, float t) const;
};
