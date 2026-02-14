#include "sequence_recorder.h"
#include "servo_controller.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <new>

void SequenceRecorder::init(ServoController* servoCtrl) {
    _servoCtrl = servoCtrl;
    _frames = new(std::nothrow) SequenceFrame[SEQUENCE_MAX_FRAMES];
    if (!_frames) {
        Serial.printf("[Seq] Failed to allocate %d frames, free heap: %lu\n",
                      SEQUENCE_MAX_FRAMES, (unsigned long)ESP.getFreeHeap());
        Serial.println("[Seq] Recorder disabled");
    }
    _frameCount = 0;
    _recording = false;
    _playing = false;
    if (_frames) Serial.println("[Seq] Recorder initialized");
}

void SequenceRecorder::update() {
    unsigned long now = millis();

    // Recording: capture frames at fixed interval
    if (_recording) {
        if (now - _lastCaptureMs >= SEQUENCE_FRAME_MS) {
            _lastCaptureMs = now;
            _captureFrame();
        }
    }

    // Playback: advance frame based on timing
    if (_playing && _frameCount > 0) {
        unsigned long elapsed = now - _playStartMs;

        // Find the frame to play based on elapsed time
        while (_playIndex < _frameCount &&
               _frames[_playIndex].timestampMs <= elapsed) {
            // Apply this frame's angles
            _servoCtrl->setAllAngles(_frames[_playIndex].angles, NUM_SERVOS);
            _playIndex++;
        }

        // Check if playback is done
        if (_playIndex >= _frameCount) {
            if (_looping) {
                _playIndex = 0;
                _playStartMs = now;
            } else {
                _playing = false;
                Serial.println("[Seq] Playback finished");
            }
        }
    }
}

void SequenceRecorder::startRecording() {
    if (!_frames) return;
    stopPlayback();
    _frameCount = 0;
    _recording = true;
    _recStartMs = millis();
    _lastCaptureMs = _recStartMs;
    _captureFrame(); // Capture first frame immediately
    Serial.println("[Seq] Recording started");
}

void SequenceRecorder::stopRecording() {
    if (!_recording) return;
    _recording = false;
    Serial.printf("[Seq] Recording stopped, %d frames captured\n", _frameCount);
}

bool SequenceRecorder::loadSequence(const char* name) {
    String path = _seqFilePath(name);
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[Seq] Cannot open %s\n", path.c_str());
        return false;
    }

    // Parse JSON: {"frames":[{"t":0,"a":[90,90,...]}, ...]}
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[Seq] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray frames = doc["frames"];
    _frameCount = 0;
    for (JsonObject frame : frames) {
        if (_frameCount >= SEQUENCE_MAX_FRAMES) break;
        _frames[_frameCount].timestampMs = frame["t"] | 0;
        JsonArray angles = frame["a"];
        for (uint8_t i = 0; i < NUM_SERVOS; i++) {
            _frames[_frameCount].angles[i] = (i < angles.size()) ? angles[i].as<uint8_t>() : 90;
        }
        _frameCount++;
    }

    Serial.printf("[Seq] Loaded '%s': %d frames\n", name, _frameCount);
    return _frameCount > 0;
}

void SequenceRecorder::startPlayback(bool loop) {
    if (_frameCount == 0) return;
    stopRecording();
    _playing = true;
    _looping = loop;
    _playIndex = 0;
    _playStartMs = millis();
    Serial.printf("[Seq] Playback started (%d frames, loop=%d)\n", _frameCount, loop);
}

void SequenceRecorder::stopPlayback() {
    if (!_playing) return;
    _playing = false;
    Serial.println("[Seq] Playback stopped");
}

bool SequenceRecorder::saveSequence(const char* name) {
    if (_frameCount == 0) {
        Serial.println("[Seq] Nothing to save");
        return false;
    }

    // Ensure directory exists
    if (!LittleFS.exists(SEQUENCE_DIR)) {
        LittleFS.mkdir(SEQUENCE_DIR);
    }

    String path = _seqFilePath(name);
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.printf("[Seq] Cannot create %s\n", path.c_str());
        return false;
    }

    // Write JSON in streaming fashion to save memory
    file.print("{\"frames\":[");
    for (uint16_t i = 0; i < _frameCount; i++) {
        if (i > 0) file.print(",");
        file.printf("{\"t\":%lu,\"a\":[", (unsigned long)_frames[i].timestampMs);
        for (uint8_t j = 0; j < NUM_SERVOS; j++) {
            if (j > 0) file.print(",");
            file.print(_frames[i].angles[j]);
        }
        file.print("]}");
    }
    file.print("]}");
    file.close();

    Serial.printf("[Seq] Saved '%s': %d frames\n", name, _frameCount);
    return true;
}

bool SequenceRecorder::deleteSequence(const char* name) {
    String path = _seqFilePath(name);
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        Serial.printf("[Seq] Deleted '%s'\n", name);
        return true;
    }
    return false;
}

String SequenceRecorder::listSequencesJson() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    File root = LittleFS.open(SEQUENCE_DIR);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            // Strip .json extension for display
            if (name.endsWith(".json")) {
                name = name.substring(0, name.length() - 5);
            }
            arr.add(name);
            file = root.openNextFile();
        }
    }

    String json;
    serializeJson(doc, json);
    return json;
}

void SequenceRecorder::_captureFrame() {
    if (_frameCount >= SEQUENCE_MAX_FRAMES) {
        stopRecording();
        return;
    }

    SequenceFrame& f = _frames[_frameCount];
    f.timestampMs = millis() - _recStartMs;
    _servoCtrl->getAllAngles(f.angles, NUM_SERVOS);
    _frameCount++;
}

String SequenceRecorder::_seqFilePath(const char* name) {
    return String(SEQUENCE_DIR) + "/" + String(name) + ".json";
}
