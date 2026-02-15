#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "output_controller.h"
#include "wifi_controller.h"
#include "ps4_input.h"
#include "analog_input.h"

OutputController  outputController;
WiFiController   wifiController;
PS4Input         ps4Input;
AnalogInput      analogInput;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Output Controller Starting ===");

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed!");
    } else {
        Serial.println("[FS] LittleFS mounted");
        // Ensure directories exist
        if (!LittleFS.exists("/presets")) LittleFS.mkdir("/presets");
        if (!LittleFS.exists("/seqdata")) LittleFS.mkdir("/seqdata");
        if (!LittleFS.exists("/envdata")) LittleFS.mkdir("/envdata");
    }

    // Initialize output controller (PCA9685)
    outputController.init();

    // Initialize WiFi + Web UI + REST API
    wifiController.init(&outputController);

    // Initialize PS4 controller (Bluetooth Classic)
    ps4Input.init(&outputController);

    // Initialize analog inputs
    analogInput.init(&outputController);

    Serial.printf("[Heap] Free: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("[Log] Level: %d (%s)\n", LOG_LEVEL,
                  LOG_LEVEL == 0 ? "ERROR" : LOG_LEVEL == 1 ? "INFO" : "DEBUG");
    Serial.println("=== Output Controller Ready ===\n");
}

void loop() {
    ps4Input.update();
    analogInput.update();
    outputController.update();

#if LOG_LEVEL >= 2
    static unsigned long _lastDumpMs = 0;
    unsigned long now = millis();
    if (now - _lastDumpMs >= 5000) {
        _lastDumpMs = now;
        String line = "[" + String(now) + "] INPUTS:";
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
            const OutputChannel& ch = outputController.getChannel(i);
            if (ch.inputSource != INPUT_MANUAL) {
                line += " ch" + String(i) + "=" + String(ch.inputSource);
            }
        }
        line += " | PS4=" + String(ps4Input.isConnected() ? "YES" : "NO");
        Serial.println(line.c_str());
    }
#endif
}
