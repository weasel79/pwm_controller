#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include "config.h"

struct ServoChannel {
    uint8_t  channel;
    uint16_t minUs;
    uint16_t maxUs;
    uint8_t  currentAngle;
    uint8_t  targetAngle;
    char     name[16];
    bool     enabled;
};

class ServoController {
public:
    void init();
    void setAngle(uint8_t channel, uint8_t angle);
    uint8_t getAngle(uint8_t channel) const;
    void setAllAngles(const uint8_t angles[], uint8_t count);
    void getAllAngles(uint8_t outAngles[], uint8_t count) const;
    const ServoChannel& getChannel(uint8_t ch) const;
    void setChannelRange(uint8_t channel, uint16_t minUs, uint16_t maxUs);
    void setChannelName(uint8_t channel, const char* name);
    void setSmoothingSteps(uint8_t steps);
    void update(); // Call in loop() for smooth movement

private:
    Adafruit_PWMServoDriver _pca = Adafruit_PWMServoDriver(PCA9685_ADDR);
    ServoChannel _channels[NUM_SERVOS];
    uint8_t _smoothSteps = 0; // 0 = instant
    void _writeMicroseconds(uint8_t channel, uint16_t us);
    uint16_t _angleToPulse(uint8_t channel, uint8_t angle) const;
};
