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
};

enum OutputType : uint8_t {
    OUTPUT_SERVO = 0,   // Standard servo: 500-2500us pulse, 0-180 = angle
    OUTPUT_MOTOR = 1,   // DC motor: full PWM duty cycle, 0-180 = speed %
    OUTPUT_LEGO  = 2,   // Lego motor: full PWM duty cycle, 0-180 = speed %
    OUTPUT_PWM      = 3,   // Raw PWM: full duty cycle, 0-180 = 0-100%
    OUTPUT_MOTOR_1K = 4    // DC motor @ 1kHz: full duty cycle, 0-180 = speed %
};

struct OutputChannel {
    uint8_t    channel;
    OutputType type;
    uint16_t   minUs;
    uint16_t   maxUs;
    uint8_t    currentValue;
    uint8_t    targetValue;
    char       name[16];
    bool       enabled;
    InputSource inputSource;
};

class OutputController {
public:
    void init();
    void setValue(uint8_t channel, uint8_t value);
    uint8_t getValue(uint8_t channel) const;
    void setAllValues(const uint8_t values[], uint8_t count);
    void getAllValues(uint8_t outValues[], uint8_t count) const;
    const OutputChannel& getChannel(uint8_t ch) const;
    void setChannelRange(uint8_t channel, uint16_t minUs, uint16_t maxUs);
    void setChannelName(uint8_t channel, const char* name);
    void setChannelType(uint8_t channel, OutputType type);
    void setChannelInput(uint8_t channel, InputSource src);
    void setSmoothingSteps(uint8_t steps);
    void update(); // Call in loop() for smooth movement

private:
    Adafruit_PWMServoDriver _pca = Adafruit_PWMServoDriver(PCA9685_ADDR);
    OutputChannel _channels[NUM_OUTPUTS];
    uint8_t _smoothSteps = 0; // 0 = instant
    uint16_t _currentFreqHz = OUTPUT_FREQ_HZ;
    void _writePWM(uint8_t channel, uint8_t value);
    void _updateFrequency();
    uint16_t _servoValueToPulse(uint8_t channel, uint8_t value) const;
};
