#include "wifi_controller.h"
#include "servo_controller.h"
#include "sequence_recorder.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ArduinoJson.h>
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

void WiFiController::init(ServoController* servoCtrl, SequenceRecorder* seqRec) {
    _servoCtrl = servoCtrl;
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
    ServoController* sc = _servoCtrl;
    SequenceRecorder* sr = _seqRec;

    // Serve Web UI
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", WEB_UI_HTML);
    });

    // GET /api/servos - get all servo positions
    _server.on("/api/servos", HTTP_GET, [sc](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (uint8_t i = 0; i < NUM_SERVOS; i++) {
            JsonObject obj = arr.add<JsonObject>();
            const ServoChannel& ch = sc->getChannel(i);
            obj["channel"] = ch.channel;
            obj["angle"] = ch.currentAngle;
            obj["name"] = ch.name;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST /api/servo - set single servo
    _server.on("/api/servo", HTTP_POST,
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
            sc->setAngle(channel, angle);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/servos - set multiple servos
    _server.on("/api/servos", HTTP_POST,
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
            for (size_t i = 0; i < angles.size() && i < NUM_SERVOS; i++) {
                sc->setAngle(i, angles[i].as<uint8_t>());
            }
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
}
