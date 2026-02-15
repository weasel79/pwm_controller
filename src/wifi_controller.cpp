#include "wifi_controller.h"
#include "output_controller.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_wifi.h>
#include <esp_coexist.h>
#include <esp_event.h>
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

void WiFiController::init(OutputController* outputCtrl) {
    _outputCtrl = outputCtrl;
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
        } else {
            Serial.println("[WiFi] STA connection failed, falling back to AP mode");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
            delay(100);
            esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
            Serial.print("[WiFi] AP started, IP: ");
            Serial.println(WiFi.softAPIP());
        }
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
        delay(500);
        WiFi.setSleep(false);
        esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
        esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
        Serial.print("[WiFi] AP started: ");
        Serial.print(WIFI_AP_SSID);
        Serial.print(", Password: ");
        Serial.print(WIFI_AP_PASSWORD);
        Serial.print(", IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

void WiFiController::_setupRoutes() {
    OutputController* sc = _outputCtrl;

    // Serve Web UI
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", WEB_UI_HTML);
    });

    // GET /api/outputs - get all output channel states
    _server.on("/api/outputs", HTTP_GET, [sc](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
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
                case OUTPUT_MOTOR_1K: obj["type"] = "motor1k";  break;
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
            }
            obj["pot"] = ch.potIndex;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST /api/output - set single output
    _server.on("/api/output", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            float angle = doc["angle"] | 90.0f;
            sc->setValue(channel, angle);
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
            else if (strcmp(typeStr, "motor1k") == 0) type = OUTPUT_MOTOR_1K;
            else {
                request->send(400, "application/json", "{\"error\":\"invalid type\"}");
                return;
            }
            sc->setChannelType(channel, type);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/input - set channel input source
    _server.on("/api/input", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
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
            else {
                request->send(400, "application/json", "{\"error\":\"invalid input\"}");
                return;
            }
            sc->setChannelInput(channel, src);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/pot-assign - set which pot drives a channel
    _server.on("/api/pot-assign", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            uint8_t pot = doc["pot"] | 0;
            sc->setChannelPot(channel, pot);
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
        request->send(200, "application/json", json);
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
        request->send(200, "application/json", json);
    });

    // POST /api/preset-save
    _server.on("/api/preset-save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
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
            String presetJson;
            sc->getPresetJson(presetJson);
            File file = LittleFS.open(path, "w");
            if (!file) {
                request->send(500, "application/json", "{\"error\":\"file create failed\"}");
                return;
            }
            file.print(presetJson);
            file.close();
            Serial.printf("[Preset] Saved '%s'\n", name);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/preset-load
    _server.on("/api/preset-load", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
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
            free(buf);
            if (ok) {
                Serial.printf("[Preset] Loaded '%s'\n", name);
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
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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
            sc->playCurve(channel, curve, count, duration, loop);
            free(curve);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/curve-stop - stop curve playback
    _server.on("/api/curve-stop", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sc](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            uint8_t channel = doc["channel"] | 0;
            sc->stopCurve(channel);
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
        request->send(200, "application/json", json);
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
}
