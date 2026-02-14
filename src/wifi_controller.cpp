#include "wifi_controller.h"
#include "output_controller.h"
#include "sequence_recorder.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_wifi.h>
#include <esp_coexist.h>
#include <esp_event.h>

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

void WiFiController::init(OutputController* outputCtrl, SequenceRecorder* seqRec) {
    _outputCtrl = outputCtrl;
    _seqRec = seqRec;
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
        // Disable WiFi sleep - keeps radio active for reliable WPA2 handshake
        WiFi.setSleep(false);
        // Force 802.11b/g only - better BLE coexistence than 11n
        esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
        // Prefer WiFi over BLE when both are active
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
    SequenceRecorder* sr = _seqRec;

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
            }
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
            uint8_t angle = doc["angle"] | 90;
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
                sc->setValue(i, angles[i].as<uint8_t>());
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
            if (strcmp(typeStr, "servo") == 0) {
                type = OUTPUT_SERVO;
            } else if (strcmp(typeStr, "motor") == 0) {
                type = OUTPUT_MOTOR;
            } else if (strcmp(typeStr, "lego") == 0) {
                type = OUTPUT_LEGO;
            } else if (strcmp(typeStr, "pwm") == 0) {
                type = OUTPUT_PWM;
            } else if (strcmp(typeStr, "motor1k") == 0) {
                type = OUTPUT_MOTOR_1K;
            } else {
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
            else {
                request->send(400, "application/json", "{\"error\":\"invalid input\"}");
                return;
            }
            sc->setChannelInput(channel, src);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/sequence/record
    _server.on("/api/sequence/record", HTTP_POST, [sr](AsyncWebServerRequest* request) {
        sr->startRecording();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // POST /api/sequence/stop
    _server.on("/api/sequence/stop", HTTP_POST, [sr](AsyncWebServerRequest* request) {
        sr->stopRecording();
        sr->stopPlayback();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // POST /api/sequence/play
    _server.on("/api/sequence/play", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sr](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "_last";
            bool loop = doc["loop"] | false;
            if (sr->loadSequence(name)) {
                sr->startPlayback(loop);
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(404, "application/json", "{\"error\":\"sequence not found\"}");
            }
        }
    );

    // POST /api/sequence/save
    _server.on("/api/sequence/save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sr](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            const char* name = doc["name"] | "unnamed";
            if (sr->saveSequence(name)) {
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(500, "application/json", "{\"error\":\"save failed\"}");
            }
        }
    );

    // GET /api/sequences - list saved sequences
    _server.on("/api/sequences", HTTP_GET, [sr](AsyncWebServerRequest* request) {
        String json = sr->listSequencesJson();
        request->send(200, "application/json", json);
    });

    // POST /api/sequence/delete
    _server.on("/api/sequence/delete", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [sr](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
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
            if (sr->deleteSequence(name)) {
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
