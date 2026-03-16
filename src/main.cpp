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

    // Initialize digital inputs
    digitalInput.init(&outputController);

    // Clean BT state in case of soft reboot (watchdog/crash leaves stale BT controller).
    // Without this, esp_spp_init() tries to delete a NULL queue → vQueueDelete assert.
    if (btStarted()) {
        Serial.println("[BT] Cleaning stale BT state from soft reboot");
        btStop();
        delay(100);
    }

    // Initialize PS4 + MK BEFORE WiFi — BT controller init crashes if WiFi STA
    // is already connected (radio contention during Bluedroid init).
    ps4Input.init(&outputController);
    mkInput.init(&ps4Input);

    // Initialize WiFi + Web UI + REST API (after BT is fully initialized)
    wifiController.init(&outputController, &digitalInput, &ps4Input, &mkInput);

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
        // Only trigger when WiFi has clients (STA=connected to router, AP=client joined).
        // Prevents reboot loop when in AP mode with nobody connected yet.
        bool hasWifiClients = (WiFi.getMode() == WIFI_STA && WiFi.isConnected())
                           || (WiFi.getMode() == WIFI_AP && WiFi.softAPgetStationNum() > 0);
        if (reqDelta == 0 && setDelta == 0 && now > 30000 && hasWifiClients) {
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
