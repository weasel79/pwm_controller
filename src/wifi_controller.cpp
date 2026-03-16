#include "wifi_controller.h"
#include "output_controller.h"
#include "digital_input.h"
#include "ps4_input.h"
#include "mk_input.h"
#include "web_ui_gz.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_wifi.h>
#include <esp_coexist.h>
#include <esp_event.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#define PRESET_DIR "/presets"

static void _wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
            auto& c = info.wifi_ap_staconnected;
            Serial.printf("[WiFi] Client connected: %02X:%02X:%02X:%02X:%02X:%02X (AID=%d)\n",
                          c.mac[0], c.mac[1], c.mac[2], c.mac[3], c.mac[4], c.mac[5], c.aid);
            break;
        }
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
            auto& d = info.wifi_ap_stadisconnected;
            Serial.printf("[WiFi] Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X (AID=%d)\n",
                          d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5], d.aid);
            break;
        }
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: {
            Serial.println("[WiFi] Probe request received");
            break;
        }
        default:
            Serial.printf("[WiFi] Event: %d\n", event);
            break;
    }
}

void WiFiController::init(OutputController* outputCtrl, DigitalInput* digitalInput, PS4Input* ps4Input, MkInput* mkInput) {
    _outputCtrl = outputCtrl;
    _digitalInput = digitalInput;
    _ps4Input = ps4Input;
    _mkInput = mkInput;
    WiFi.onEvent(_wifiEventHandler);
    _setupWiFi();
    _setupRoutes();
    _server.begin();
    Serial.println("[WiFi] Web server started on port " + String(WEB_SERVER_PORT));
}

void WiFiController::_setupWiFi() {
    if (WIFI_STA_MODE) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.print("[WiFi] Connecting to ");
        Serial.print(WIFI_SSID);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("[WiFi] Connected, IP: ");
            Serial.println(WiFi.localIP());
            if (MDNS.begin("pwm")) {
                MDNS.addService("http", "tcp", WEB_SERVER_PORT);
                Serial.println("[WiFi] mDNS: http://pwm.local");
            }
        } else {
            Serial.println("[WiFi] STA connection failed, falling back to AP mode");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
            delay(100);
            Serial.print("[WiFi] AP started, IP: ");
            Serial.println(WiFi.softAPIP());
        }
        // Balance WiFi/BT coexistence (PREFER_BT not available in this ESP-IDF).
        esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
        delay(500);
        WiFi.setSleep(false);
        esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
        esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
        Serial.print("[WiFi] AP started: ");
        Serial.print(WIFI_AP_SSID);
        Serial.print(", Password: ");
        Serial.print(WIFI_AP_PASSWORD);
        Serial.print(", IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

// Diagnostics counters (defined in main.cpp)
extern volatile uint32_t g_httpReqCount;
extern volatile uint32_t g_httpSetCount;

// Helper: send JSON with Connection: close to free TCP sockets faster
static void sendJsonClose(AsyncWebServerRequest* req, int code, const String& json) {
    g_httpReqCount++;
    AsyncWebServerResponse* resp = req->beginResponse(code, "application/json", json);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

void WiFiController::_setupRoutes() {
    OutputController* sc = _outputCtrl;
    DigitalInput* di = _digitalInput;
    PS4Input* ps4 = _ps4Input;
    MkInput* mk = _mkInput;

    // Serve Web UI (gzip compressed: 60KB → 13KB for fast page loads)
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse_P(200, "text/html", WEB_UI_GZ, WEB_UI_GZ_LEN);
        resp->addHeader("Content-Encoding", "gzip");
        request->send(resp);
    });

    // GET /api/state - combined endpoint: outputs + raw inputs in one request.
    // Eliminates multiple concurrent HTTP requests that cause TCP socket exhaustion.
    // GET /api/state - lightweight poll: just angles + PS4 connected + MK count
    // Full config is fetched once via /api/outputs on page load
    _server.on("/api/state", HTTP_GET, [sc, ps4, mk](AsyncWebServerRequest* request) {
        // Build compact response manually (no ArduinoJson = no heap pressure)
        String json = "{\"a\":[";
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
            if (i) json += ',';
            json += String((int)roundf(sc->getChannel(i).currentValue));
        }
        if (mk && mk->isConnected()) {
            for (uint8_t i = 0; i < NUM_MK_CHANNELS; i++) {
                json += ',';
                json += String((int)roundf(mk->getValue(i)));
            }
        }
        json += "],\"ps4\":";
        json += (ps4 && ps4->isConnected()) ? '1' : '0';
        json += ",\"mk\":";
        json += (mk && mk->isConnected()) ? '1' : '0';
        json += '}';
        sendJsonClose(request, 200, json);
    });

    // GET /api/outputs - get all output channel states (16 PCA + 6 MK when connected)
    _server.on("/api/outputs", HTTP_GET, [sc, mk](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        // PCA9685 channels 0-15
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
            JsonObject obj = arr.add<JsonObject>();
            const OutputChannel& ch = sc->getChannel(i);
            obj["channel"] = ch.channel;
            obj["angle"] = ch.currentValue;
            obj["name"] = ch.name;
            switch (ch.type) {
                case OUTPUT_SERVO: obj["type"] = "servo"; break;
                case OUTPUT_MOTOR: obj["type"] = "motor"; break;
                case OUTPUT_LEGO:  obj["type"] = "lego";  break;
                case OUTPUT_PWM:      obj["type"] = "pwm";      break;
            }
            switch (ch.inputSource) {
                case INPUT_MANUAL:     obj["input"] = "manual";     break;
                case INPUT_ENVELOPE:   obj["input"] = "envelope";   break;
                case INPUT_POT:        obj["input"] = "pot";        break;
                case INPUT_PS4_LX:     obj["input"] = "ps4_lx";     break;
                case INPUT_PS4_LY:     obj["input"] = "ps4_ly";     break;
                case INPUT_PS4_RX:     obj["input"] = "ps4_rx";     break;
                case INPUT_PS4_RY:     obj["input"] = "ps4_ry";     break;
                case INPUT_PS4_L2:     obj["input"] = "ps4_l2";     break;
                case INPUT_PS4_R2:     obj["input"] = "ps4_r2";     break;
                case INPUT_PS4_CROSS:  obj["input"] = "ps4_cross";  break;
                case INPUT_PS4_CIRCLE: obj["input"] = "ps4_circle"; break;
                case INPUT_PS4_SQUARE: obj["input"] = "ps4_square"; break;
                case INPUT_PS4_TRIANGLE: obj["input"] = "ps4_triangle"; break;
                case INPUT_PS4_L1:     obj["input"] = "ps4_l1";     break;
                case INPUT_PS4_R1:     obj["input"] = "ps4_r1";     break;
                case INPUT_SEQUENCE:   obj["input"] = "sequence";   break;
                case INPUT_DIGITAL:    obj["input"] = "digital";    break;
            }
            obj["pot"] = ch.potIndex;
            obj["pwmMin"] = ch.pwmMin;
            obj["pwmMax"] = ch.pwmMax;
            obj["digPin"] = ch.digitalPin;
            obj["digMode"] = ch.digitalMode;
        }
        // MK channels 16-21 (only when connected)
        if (mk && mk->isConnected()) {
            static const char* MK_NAMES[] = {"MK A","MK B","MK C","MK D","MK E","MK F"};
            for (uint8_t i = 0; i < NUM_MK_CHANNELS; i++) {
                JsonObject obj = arr.add<JsonObject>();
                obj["channel"] = NUM_OUTPUTS + i;
                obj["angle"] = mk->getValue(i);
                obj["name"] = MK_NAMES[i];
                obj["type"] = "motor";
                // Use same input source name mapping as PCA channels
                InputSource mkSrc = mk->getInputSource(i);
                const char* mkSrcName = "manual";
                switch (mkSrc) {
                    case INPUT_MANUAL: mkSrcName = "manual"; break;
                    case INPUT_ENVELOPE: mkSrcName = "envelope"; break;
                    case INPUT_POT: mkSrcName = "pot"; break;
                    case INPUT_PS4_LX: mkSrcName = "ps4_lx"; break;
                    case INPUT_PS4_LY: mkSrcName = "ps4_ly"; break;
                    case INPUT_PS4_RX: mkSrcName = "ps4_rx"; break;
                    case INPUT_PS4_RY: mkSrcName = "ps4_ry"; break;
                    case INPUT_PS4_L2: mkSrcName = "ps4_l2"; break;
                    case INPUT_PS4_R2: mkSrcName = "ps4_r2"; break;
                    case INPUT_PS4_CROSS: mkSrcName = "ps4_cross"; break;
                    case INPUT_PS4_CIRCLE: mkSrcName = "ps4_circle"; break;
                    case INPUT_PS4_SQUARE: mkSrcName = "ps4_square"; break;
                    case INPUT_PS4_TRIANGLE: mkSrcName = "ps4_triangle"; break;
                    case INPUT_PS4_L1: mkSrcName = "ps4_l1"; break;
                    case INPUT_PS4_R1: mkSrcName = "ps4_r1"; break;
                    case INPUT_SEQUENCE: mkSrcName = "sequence"; break;
                    case INPUT_DIGITAL: mkSrcName = "digital"; break;
                }
                obj["input"] = mkSrcName;
                obj["pot"] = mk->getPotIndex(i);
                obj["pwmMin"] = 0;
                obj["pwmMax"] = 4095;
                obj["digPin"] = 0;
                obj["digMode"] = 0;
            }
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // POST /api/output - set single output (ch 0-15 = PCA, 16-21 = MK)
    _server.on("/api/output", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            float angle = doc["angle"] | 90.0f;
            g_httpSetCount++;
            if (channel >= NUM_OUTPUTS && mk) {
                mk->setValue(channel - NUM_OUTPUTS, angle);
            } else {
                sc->setValue(channel, angle);
            }
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/outputs - set multiple outputs
    _server.on("/api/outputs", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            JsonArray angles = doc["angles"];
            if (angles.isNull()) {
                request->send(400, "application/json", "{\"error\":\"missing angles array\"}");
                return;
            }
            for (size_t i = 0; i < angles.size() && i < NUM_OUTPUTS; i++) {
                sc->setValue(i, angles[i].as<float>());
            }
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/output/type - set channel output type
    _server.on("/api/output/type", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            const char* typeStr = doc["type"] | "";
            OutputType type;
            if (strcmp(typeStr, "servo") == 0) type = OUTPUT_SERVO;
            else if (strcmp(typeStr, "motor") == 0) type = OUTPUT_MOTOR;
            else if (strcmp(typeStr, "lego") == 0) type = OUTPUT_LEGO;
            else if (strcmp(typeStr, "pwm") == 0) type = OUTPUT_PWM;
            else {
                request->send(400, "application/json", "{\"error\":\"invalid type\"}");
                return;
            }
            sc->setChannelType(channel, type);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/input - set channel input source (ch 0-15 = PCA, 16-21 = MK)
    _server.on("/api/input", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            const char* inStr = doc["input"] | "";
            InputSource src;
            if (strcmp(inStr, "manual") == 0) src = INPUT_MANUAL;
            else if (strcmp(inStr, "envelope") == 0) src = INPUT_ENVELOPE;
            else if (strcmp(inStr, "pot") == 0) src = INPUT_POT;
            else if (strcmp(inStr, "ps4_lx") == 0) src = INPUT_PS4_LX;
            else if (strcmp(inStr, "ps4_ly") == 0) src = INPUT_PS4_LY;
            else if (strcmp(inStr, "ps4_rx") == 0) src = INPUT_PS4_RX;
            else if (strcmp(inStr, "ps4_ry") == 0) src = INPUT_PS4_RY;
            else if (strcmp(inStr, "ps4_l2") == 0) src = INPUT_PS4_L2;
            else if (strcmp(inStr, "ps4_r2") == 0) src = INPUT_PS4_R2;
            else if (strcmp(inStr, "ps4_cross") == 0) src = INPUT_PS4_CROSS;
            else if (strcmp(inStr, "ps4_circle") == 0) src = INPUT_PS4_CIRCLE;
            else if (strcmp(inStr, "ps4_square") == 0) src = INPUT_PS4_SQUARE;
            else if (strcmp(inStr, "ps4_triangle") == 0) src = INPUT_PS4_TRIANGLE;
            else if (strcmp(inStr, "ps4_l1") == 0) src = INPUT_PS4_L1;
            else if (strcmp(inStr, "ps4_r1") == 0) src = INPUT_PS4_R1;
            else if (strcmp(inStr, "sequence") == 0) src = INPUT_SEQUENCE;
            else if (strcmp(inStr, "digital") == 0) src = INPUT_DIGITAL;
            else {
                request->send(400, "application/json", "{\"error\":\"invalid input\"}");
                return;
            }
            if (channel >= NUM_OUTPUTS && mk) {
                mk->setInputSource(channel - NUM_OUTPUTS, src);
            } else {
                sc->setChannelInput(channel, src);
            }
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/pot-assign - set which pot drives a channel
    _server.on("/api/pot-assign", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            uint8_t pot = doc["pot"] | 0;
            if (channel >= NUM_OUTPUTS && mk) {
                mk->setPotIndex(channel - NUM_OUTPUTS, pot);
            } else {
                sc->setChannelPot(channel, pot);
            }
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/pwm-range - set PWM tick range for a channel
    _server.on("/api/pwm-range", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            uint16_t pwmMin = doc["min"] | 0;
            uint16_t pwmMax = doc["max"] | 4095;
            sc->setChannelRange(channel, pwmMin, pwmMax);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/digital-config - set digital pin and mode for a channel
    _server.on("/api/digital-config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            if (channel >= NUM_OUTPUTS) {
                request->send(400, "application/json", "{\"error\":\"invalid channel\"}");
                return;
            }
            // Access channel directly via getChannel (const) — need non-const access
            // Use output controller methods instead
            OutputChannel& ch = const_cast<OutputChannel&>(sc->getChannel(channel));
            ch.digitalPin = doc["pin"] | 0;
            ch.digitalMode = doc["mode"] | 0;
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // GET /api/digital-state - get current digital pin states
    _server.on("/api/digital-state", HTTP_GET, [di](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc["pins"].to<JsonArray>();
        for (uint8_t i = 0; i < NUM_POTS; i++) {
            arr.add(di ? di->getPinState(i) : false);
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // GET /api/raw-inputs - get raw hardware input values (PS4, analog, digital)
    // IMPORTANT: Read from cached PS4State (updated in main loop) — never call
    // PS4 library directly from the web server task (causes BT HCI crash).
    _server.on("/api/raw-inputs", HTTP_GET, [di, ps4](AsyncWebServerRequest* request) {
        JsonDocument doc;
        // PS4 controller — read from cached state (thread-safe)
        JsonObject ps4obj = doc["ps4"].to<JsonObject>();
        if (ps4) {
            const PS4State& s = ps4->getState();
            ps4obj["connected"] = s.connected;
            ps4obj["lx"] = s.lx;
            ps4obj["ly"] = s.ly;
            ps4obj["rx"] = s.rx;
            ps4obj["ry"] = s.ry;
            ps4obj["l2"] = s.l2;
            ps4obj["r2"] = s.r2;
            ps4obj["cross"] = s.cross;
            ps4obj["circle"] = s.circle;
            ps4obj["square"] = s.square;
            ps4obj["triangle"] = s.triangle;
            ps4obj["l1"] = s.l1;
            ps4obj["r1"] = s.r1;
        } else {
            ps4obj["connected"] = false;
            ps4obj["lx"] = 0; ps4obj["ly"] = 0;
            ps4obj["rx"] = 0; ps4obj["ry"] = 0;
            ps4obj["l2"] = 0; ps4obj["r2"] = 0;
            ps4obj["cross"] = false; ps4obj["circle"] = false;
            ps4obj["square"] = false; ps4obj["triangle"] = false;
            ps4obj["l1"] = false; ps4obj["r1"] = false;
        }
        // Analog inputs (raw ADC values 0-4095)
        JsonArray analog = doc["analog"].to<JsonArray>();
        for (uint8_t i = 0; i < NUM_POTS; i++) {
            analog.add(analogRead(POT_PINS[i]));
        }
        // Digital inputs (debounced pin states)
        JsonArray digital = doc["digital"].to<JsonArray>();
        for (uint8_t i = 0; i < NUM_POTS; i++) {
            digital.add(di ? di->getPinState(i) : false);
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // POST /api/global-freq - set global PWM frequency
    _server.on("/api/global-freq", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint16_t freq = doc["freq"] | 50;
            sc->setGlobalFrequency(freq);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // GET /api/global-freq - get current PWM frequency
    _server.on("/api/global-freq", HTTP_GET, [sc](AsyncWebServerRequest* request) {
        String json = "{\"freq\":" + String(sc->getGlobalFrequency()) + "}";
        sendJsonClose(request, 200, json);
    });

    // POST /api/stop-all - stop all curve playback
    _server.on("/api/stop-all", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            sc->stopAllCurves();
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/seq-save - save per-channel sequence data
    _server.on("/api/seq-save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            JsonArray pts = doc["points"];
            if (pts.isNull()) {
                request->send(400, "application/json", "{\"error\":\"missing points\"}");
                return;
            }
            String path = String(SEQDATA_DIR) + "/" + String(name) + ".json";
            File file = LittleFS.open(path, "w");
            if (!file) {
                request->send(500, "application/json", "{\"error\":\"file create failed\"}");
                return;
            }
            // Write compact JSON: {ch:0, pts:[[t,a],[t,a],...]}
            file.printf("{\"ch\":%d,\"pts\":[", channel);
            for (size_t i = 0; i < pts.size(); i++) {
                if (i > 0) file.print(",");
                float t = pts[i]["t"] | 0.0f;
                uint8_t a = pts[i]["a"] | 90;
                file.printf("[%.3f,%d]", t, a);
            }
            file.print("]}");
            file.close();
            Serial.printf("[Seq] Saved '%s' ch%d (%d pts)\n", name, channel, pts.size());
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/seq-load - load per-channel sequence data
    _server.on("/api/seq-load", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String(SEQDATA_DIR) + "/" + String(name) + ".json";
            File file = LittleFS.open(path, "r");
            if (!file) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
                return;
            }
            String content = file.readString();
            file.close();
            request->send(200, "application/json", content);
        }
    );

    // GET /api/seq-list - list saved sequences
    _server.on("/api/seq-list", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        File root = LittleFS.open(SEQDATA_DIR);
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                String name = file.name();
                int sl = name.lastIndexOf('/');
                if (sl >= 0) name = name.substring(sl + 1);
                if (name.endsWith(".json")) {
                    name = name.substring(0, name.length() - 5);
                }
                arr.add(name);
                file = root.openNextFile();
            }
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // POST /api/seq-del - delete a saved sequence
    _server.on("/api/seq-del", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String(SEQDATA_DIR) + "/" + String(name) + ".json";
            if (LittleFS.exists(path)) {
                LittleFS.remove(path);
                Serial.printf("[Seq] Deleted '%s'\n", name);
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
            }
        }
    );

    // GET /api/presets - list saved presets
    _server.on("/api/presets", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        File root = LittleFS.open(PRESET_DIR);
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                String name = file.name();
                int sl = name.lastIndexOf('/');
                if (sl >= 0) name = name.substring(sl + 1);
                if (name.endsWith(".json")) {
                    name = name.substring(0, name.length() - 5);
                }
                arr.add(name);
                file = root.openNextFile();
            }
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // POST /api/preset-save
    _server.on("/api/preset-save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            // Build preset with PCA + MK channels
            JsonDocument preset;
            String pcaJson;
            sc->getPresetJson(pcaJson);
            deserializeJson(preset, pcaJson);
            if (mk) {
                static const char* SN[] = {"manual","envelope","pot","ps4_lx","ps4_ly","ps4_rx","ps4_ry","ps4_l2","ps4_r2","ps4_cross","ps4_circle","ps4_square","ps4_tri","ps4_l1","ps4_r1","sequence","digital"};
                JsonArray mkArr = preset["mk"].to<JsonArray>();
                for (uint8_t i = 0; i < NUM_MK_CHANNELS; i++) {
                    JsonObject obj = mkArr.add<JsonObject>();
                    InputSource src = mk->getInputSource(i);
                    obj["input"] = (src < 17) ? SN[src] : "manual";
                    obj["value"] = (int)roundf(mk->getValue(i));
                    obj["pot"] = mk->getPotIndex(i);
                }
            }
            String path = String(PRESET_DIR) + "/" + String(name) + ".json";
            File file = LittleFS.open(path, "w");
            if (!file) {
                request->send(500, "application/json", "{\"error\":\"file create failed\"}");
                return;
            }
            serializeJson(preset, file);
            file.close();
            Serial.printf("[Preset] Saved '%s'\n", name);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/preset-load
    _server.on("/api/preset-load", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String(PRESET_DIR) + "/" + String(name) + ".json";
            File file = LittleFS.open(path, "r");
            if (!file) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
                return;
            }
            size_t fLen = file.size();
            uint8_t* buf = (uint8_t*)malloc(fLen);
            if (!buf) {
                file.close();
                request->send(500, "application/json", "{\"error\":\"out of memory\"}");
                return;
            }
            file.read(buf, fLen);
            file.close();
            bool ok = sc->applyPresetJson(buf, fLen);
            // Also restore MK channels from preset
            if (ok && mk) {
                JsonDocument preset;
                deserializeJson(preset, buf, fLen);
                JsonArray mkArr = preset["mk"];
                if (!mkArr.isNull()) {
                    static const char* SN[] = {"manual","envelope","pot","ps4_lx","ps4_ly","ps4_rx","ps4_ry","ps4_l2","ps4_r2","ps4_cross","ps4_circle","ps4_square","ps4_tri","ps4_l1","ps4_r1","sequence","digital"};
                    uint8_t mi = 0;
                    for (JsonObject obj : mkArr) {
                        if (mi >= NUM_MK_CHANNELS) break;
                        mk->setValue(mi, (float)(obj["value"] | 90));
                        mk->setPotIndex(mi, obj["pot"] | 0);
                        const char* inStr = obj["input"] | "manual";
                        InputSource src = INPUT_MANUAL;
                        for (int s = 0; s < 17; s++) {
                            if (strcmp(inStr, SN[s]) == 0) { src = (InputSource)s; break; }
                        }
                        mk->setInputSource(mi, src);
                        mi++;
                    }
                }
            }
            free(buf);
            if (ok) {
                Serial.printf("[Preset] Loaded '%s'\n", name);
                // Return compact state — just ok flag. JS will fetchState after.
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(500, "application/json", "{\"error\":\"parse failed\"}");
            }
        }
    );

    // POST /api/preset-del
    _server.on("/api/preset-del", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String(PRESET_DIR) + "/" + String(name) + ".json";
            if (LittleFS.exists(path)) {
                LittleFS.remove(path);
                Serial.printf("[Preset] Deleted '%s'\n", name);
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
            }
        }
    );

    // POST /api/curve-play - send curve points to ESP for smooth playback
    _server.on("/api/curve-play", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // Accumulate chunks for large payloads
            if (index == 0) {
                request->_tempObject = malloc(total + 1);
                if (!request->_tempObject) {
                    request->send(500, "application/json", "{\"error\":\"oom\"}");
                    return;
                }
            }
            if (!request->_tempObject) return;
            memcpy((uint8_t*)request->_tempObject + index, data, len);
            if (index + len < total) return;

            ((uint8_t*)request->_tempObject)[total] = 0;
            JsonDocument doc;
            auto err = deserializeJson(doc, (const char*)request->_tempObject, total);
            free(request->_tempObject);
            request->_tempObject = nullptr;

            if (err != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            float duration = doc["duration"] | 2.0f;
            bool loop = doc["loop"] | false;
            JsonArray pts = doc["points"];
            if (pts.isNull() || pts.size() < 2) {
                request->send(400, "application/json", "{\"error\":\"need >= 2 points\"}");
                return;
            }
            uint16_t count = pts.size();
            CurvePoint* curve = (CurvePoint*)malloc(count * sizeof(CurvePoint));
            if (!curve) {
                request->send(500, "application/json", "{\"error\":\"oom\"}");
                return;
            }
            uint16_t i = 0;
            for (JsonObject p : pts) {
                curve[i].t = p["t"] | 0.0f;
                curve[i].a = p["a"] | 90.0f;
                i++;
            }
            if (channel >= NUM_OUTPUTS && mk) {
                mk->playCurve(channel - NUM_OUTPUTS, curve, count, duration, loop);
            } else {
                sc->playCurve(channel, curve, count, duration, loop);
            }
            free(curve);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/curve-stop - stop curve playback
    _server.on("/api/curve-stop", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc, mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            if (channel >= NUM_OUTPUTS && mk) {
                mk->stopCurve(channel - NUM_OUTPUTS);
            } else {
                sc->stopCurve(channel);
            }
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/env-save - save envelope data
    _server.on("/api/env-save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                request->_tempObject = malloc(total + 1);
                if (!request->_tempObject) {
                    request->send(500, "application/json", "{\"error\":\"oom\"}");
                    return;
                }
            }
            if (!request->_tempObject) return;
            memcpy((uint8_t*)request->_tempObject + index, data, len);
            if (index + len < total) return;

            ((uint8_t*)request->_tempObject)[total] = 0;
            JsonDocument doc;
            auto err = deserializeJson(doc, (const char*)request->_tempObject, total);
            free(request->_tempObject);
            request->_tempObject = nullptr;

            if (err != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            JsonArray pts = doc["points"];
            if (pts.isNull()) {
                request->send(400, "application/json", "{\"error\":\"missing points\"}");
                return;
            }
            float duration = doc["duration"] | 2.0f;
            bool loop = doc["loop"] | false;

            String path = String("/envdata/") + String(name) + ".json";
            File file = LittleFS.open(path, "w");
            if (!file) {
                request->send(500, "application/json", "{\"error\":\"file create failed\"}");
                return;
            }
            file.printf("{\"dur\":%.3f,\"loop\":%s,\"pts\":[", duration, loop ? "true" : "false");
            for (size_t i = 0; i < pts.size(); i++) {
                if (i > 0) file.print(",");
                float t = pts[i]["t"] | 0.0f;
                float a = pts[i]["a"] | 90.0f;
                file.printf("[%.3f,%.1f]", t, a);
            }
            file.print("]}");
            file.close();
            Serial.printf("[Env] Saved '%s' (%d pts)\n", name, pts.size());
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/env-load - load envelope data
    _server.on("/api/env-load", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String("/envdata/") + String(name) + ".json";
            File file = LittleFS.open(path, "r");
            if (!file) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
                return;
            }
            String content = file.readString();
            file.close();
            request->send(200, "application/json", content);
        }
    );

    // GET /api/env-list - list saved envelopes
    _server.on("/api/env-list", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        File root = LittleFS.open("/envdata");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                String name = file.name();
                int sl = name.lastIndexOf('/');
                if (sl >= 0) name = name.substring(sl + 1);
                if (name.endsWith(".json")) {
                    name = name.substring(0, name.length() - 5);
                }
                arr.add(name);
                file = root.openNextFile();
            }
        }
        String json;
        serializeJson(doc, json);
        sendJsonClose(request, 200, json);
    });

    // POST /api/env-del - delete saved envelope
    _server.on("/api/env-del", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "";
            if (strlen(name) == 0) {
                request->send(400, "application/json", "{\"error\":\"missing name\"}");
                return;
            }
            String path = String("/envdata/") + String(name) + ".json";
            if (LittleFS.exists(path)) {
                LittleFS.remove(path);
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
            }
        }
    );

    // POST /api/ota - firmware upload
    _server.on("/api/ota", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            bool ok = !Update.hasError();
            AsyncWebServerResponse* response = request->beginResponse(
                ok ? 200 : 500, "application/json",
                ok ? "{\"ok\":true,\"msg\":\"Rebooting...\"}" : "{\"error\":\"Update failed\"}");
            response->addHeader("Connection", "close");
            request->send(response);
            if (ok) {
                delay(500);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                Serial.printf("[OTA] Begin: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (Update.isRunning()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Success: %u bytes\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    // POST /api/mk-connect - connect or disconnect MK hub {action:"connect"|"disconnect"}
    _server.on("/api/mk-connect", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [mk](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* action = doc["action"] | "connect";
            if (!mk) {
                request->send(500, "application/json", "{\"error\":\"mk not available\"}");
                return;
            }
            if (strcmp(action, "disconnect") == 0) {
                mk->disconnect();
            } else {
                mk->connect();
            }
            String json = "{\"ok\":true,\"connected\":";
            json += mk->isConnected() ? "true" : "false";
            json += "}";
            sendJsonClose(request, 200, json);
        }
    );

}
