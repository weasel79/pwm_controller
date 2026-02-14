#pragma once

#include <Arduino.h>
#include "config.h"

struct SequenceFrame {
    uint32_t timestampMs;
    uint8_t  values[NUM_OUTPUTS];
};

class OutputController;

class SequenceRecorder {
public:
    void init(OutputController* outputCtrl);
    void update(); // Call in loop()

    // Recording
    void startRecording();
    void stopRecording();
    bool isRecording() const { return _recording; }

    // Playback
    bool loadSequence(const char* name);
    void startPlayback(bool loop = false);
    void stopPlayback();
    bool isPlaying() const { return _playing; }

    // Storage
    bool saveSequence(const char* name);
    bool deleteSequence(const char* name);
    String listSequencesJson();

    uint16_t frameCount() const { return _frameCount; }
    void freeBuffer();

private:
    bool _allocBuffer();
    OutputController* _outputCtrl = nullptr;

    // Recording buffer
    SequenceFrame* _frames = nullptr;
    uint16_t _frameCount = 0;
    bool _recording = false;
    unsigned long _recStartMs = 0;
    unsigned long _lastCaptureMs = 0;

    // Playback state
    bool _playing = false;
    bool _looping = false;
    uint16_t _playIndex = 0;
    unsigned long _playStartMs = 0;

    void _captureFrame();
    String _seqFilePath(const char* name);
};
