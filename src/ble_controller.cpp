#include "ble_controller.h"
#include "servo_controller.h"
#include <NimBLEDevice.h>

static ServoController* s_servoCtrl = nullptr;
static NimBLECharacteristic* s_stateChar = nullptr;
static bool s_deviceConnected = false;

// Callback for single servo commands: "channel:angle" text, e.g. "5:90"
class ServoCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        String str = val.c_str();
        int sep = str.indexOf(':');
        if (sep > 0 && s_servoCtrl) {
            uint8_t ch = str.substring(0, sep).toInt();
            uint8_t angle = str.substring(sep + 1).toInt();
            s_servoCtrl->setAngle(ch, angle);
        }
    }
};

// Callback for bulk update: 16 bytes, one per channel (angle 0-180)
class BulkCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar) override {
        const uint8_t* data = pChar->getValue().data();
        size_t len = pChar->getValue().length();
        if (data && len > 0 && s_servoCtrl) {
            uint8_t count = min((size_t)NUM_SERVOS, len);
            for (uint8_t i = 0; i < count; i++) {
                s_servoCtrl->setAngle(i, data[i]);
            }
        }
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        s_deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }
    void onDisconnect(NimBLEServer* pServer) override {
        s_deviceConnected = false;
        Serial.println("[BLE] Client disconnected");
        NimBLEDevice::startAdvertising();
    }
};

void BLEController::init(ServoController* servoCtrl) {
    _servoCtrl = servoCtrl;
    s_servoCtrl = servoCtrl;

    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P6);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);

    // Single servo characteristic: write "ch:angle"
    NimBLECharacteristic* servoChar = pService->createCharacteristic(
        BLE_SERVO_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    servoChar->setCallbacks(new ServoCharCallbacks());

    // Bulk update characteristic: write 16 bytes
    NimBLECharacteristic* bulkChar = pService->createCharacteristic(
        BLE_BULK_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    bulkChar->setCallbacks(new BulkCharCallbacks());

    // State notification characteristic: read current angles
    s_stateChar = pService->createCharacteristic(
        BLE_STATE_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    _initialized = true;
    Serial.printf("[BLE] NimBLE server started, advertising as %s\n", BLE_DEVICE_NAME);
    Serial.printf("[Heap] After BLE: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void BLEController::updateStateNotification() {
    if (!_initialized || !s_deviceConnected || !s_stateChar || !_servoCtrl) return;

    uint8_t angles[NUM_SERVOS];
    _servoCtrl->getAllAngles(angles, NUM_SERVOS);
    s_stateChar->setValue(angles, NUM_SERVOS);
    s_stateChar->notify();
}
