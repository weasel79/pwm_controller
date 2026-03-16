#include <Arduino.h>
#include <LittleFS.h>

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

    // Initialize PS4 FIRST — it sets the BT MAC, calls btStart(), and inits Bluedroid.
    // MK then adds BLE GAP on top of the already-running Bluedroid stack.
    ps4Input.init(&outputController);

    // Initialize MouldKing BLE (uses existing Bluedroid stack).
    // Does NOT auto-connect — use the web UI "Connect MK" button to connect on demand.
    mkInput.init(&ps4Input);

    // Initialize analog inputs
    analogInput.init(&outputController);

    Serial.printf("[Heap] Free: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("[Log] Level: %d (%s)\n", LOG_LEVEL,
                  LOG_LEVEL == 0 ? "ERROR" : LOG_LEVEL == 1 ? "INFO" : "DEBUG");
    Serial.println("=== Universal PWM Control Ready ===\n");
}

// Diagnostics counters (updated by wifi_controller via extern)
volatile uint32_t g_httpReqCount = 0;
volatile uint32_t g_httpSetCount = 0;  // /api/output POST count

void loop() {
    ps4Input.update();
    analogInput.update();
    digitalInput.update();
    outputController.update();
    mkInput.update();

    // Diagnostics + TCP watchdog: print every 5s, detect stuck web server
    static unsigned long _lastDiagMs = 0;
    static uint32_t _loopCount = 0;
    static uint32_t _prevReq = 0;
    static uint32_t _prevSet = 0;
    static uint8_t _zeroReqStreak = 0;
    _loopCount++;
    unsigned long now = millis();
    if (now - _lastDiagMs >= 5000) {
        uint32_t elapsed = now - _lastDiagMs;
        uint32_t loopsPerSec = _loopCount * 1000 / elapsed;
        uint32_t reqDelta = g_httpReqCount - _prevReq;
        uint32_t setDelta = g_httpSetCount - _prevSet;
        Serial.printf("[DIAG] loop=%lu/s heap=%lu req=%lu set=%lu PS4=%s MK=%s\n",
                      (unsigned long)loopsPerSec,
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)reqDelta,
                      (unsigned long)setDelta,
                      ps4Input.isConnected() ? "Y" : "N",
                      mkInput.isConnected() ? "Y" : "N");

        // TCP watchdog: if req=0 for 4 consecutive cycles (20s), reboot.
        // This recovers from permanently stuck TCP connections.
        if (reqDelta == 0 && setDelta == 0 && now > 30000) {
            _zeroReqStreak++;
            if (_zeroReqStreak >= 4) {
                Serial.println("[WATCHDOG] Web server stuck (req=0 for 20s) — rebooting!");
                delay(100);
                ESP.restart();
            }
        } else {
            _zeroReqStreak = 0;
        }

        _loopCount = 0;
        _prevReq = g_httpReqCount;
        _prevSet = g_httpSetCount;
        _lastDiagMs = now;
    }
}
