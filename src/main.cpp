#include <Arduino.h>
#include <LittleFS.h>
#include <esp_mac.h>

#include "config.h"
#include "output_controller.h"
#include "wifi_controller.h"
#include "ps4_input.h"
#include "analog_input.h"
#include "digital_input.h"
#include "mk_input.h"

OutputController  outputController;
WiFiController   wifiController;
PS4Input         ps4Input;
AnalogInput      analogInput;
DigitalInput     digitalInput;
MkInput          mkInput;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Universal PWM Control Starting ===");

    // Set BT MAC address early, before any BT init (MK or PS4).
    // PS4 controller is paired to this MAC via SixAxis Pair Tool.
    // esp_base_mac_addr_set must be called before btStart().
    {
        uint8_t mac[6];
        sscanf(PS4_CONTROLLER_MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        uint8_t baseMac[6];
        memcpy(baseMac, mac, 6);
        baseMac[5] -= 2;  // BT MAC = base MAC + 2
        esp_base_mac_addr_set(baseMac);
        Serial.printf("[BT] Base MAC set for PS4: %s\n", PS4_CONTROLLER_MAC);
    }

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

    // Initialize digital inputs (before WiFi so pointer is ready)
    digitalInput.init(&outputController);

    // Initialize WiFi + Web UI + REST API (pass mkInput pointer for API routes)
    wifiController.init(&outputController, &digitalInput, &ps4Input, &mkInput);

    // Initialize MouldKing BLE motor controller (Bluedroid BLE + 3s connect broadcast)
    mkInput.init(&ps4Input);
    mkInput.connect();

    // Initialize PS4 controller (Bluetooth Classic, after MK connect completes)
    ps4Input.init(&outputController);

    // Initialize analog inputs
    analogInput.init(&outputController);

    Serial.printf("[Heap] Free: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("[Log] Level: %d (%s)\n", LOG_LEVEL,
                  LOG_LEVEL == 0 ? "ERROR" : LOG_LEVEL == 1 ? "INFO" : "DEBUG");
    Serial.println("=== Universal PWM Control Ready ===\n");
}

void loop() {
    ps4Input.update();
    analogInput.update();
    digitalInput.update();
    outputController.update();
    mkInput.update();

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
