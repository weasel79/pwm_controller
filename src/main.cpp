#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "servo_controller.h"
#include "wifi_controller.h"
#include "ble_controller.h"
#include "analog_input.h"
#include "sequence_recorder.h"

ServoController  servoController;
WiFiController   wifiController;
BLEController    bleController;
AnalogInput      analogInput;
SequenceRecorder sequenceRecorder;

unsigned long lastBleNotify = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Servo Controller Starting ===");

    // Initialize LittleFS for sequence storage
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed!");
    } else {
        Serial.println("[FS] LittleFS mounted");
    }

    // Initialize servo controller (PCA9685)
    servoController.init();

    // Initialize sequence recorder
    sequenceRecorder.init(&servoController);

    // Initialize WiFi + Web UI + REST API
    wifiController.init(&servoController, &sequenceRecorder);

    // Initialize BLE
    bleController.init(&servoController);

    // Initialize analog inputs
    analogInput.init(&servoController);

    // Configure default pot/joystick mappings
    // Uncomment and adjust pins/channels for your wiring:
     analogInput.addPot(POT_PIN_0, 0);   // Pot on GPIO34 -> Servo 0
    // analogInput.addPot(POT_PIN_1, 1);   // Pot on GPIO35 -> Servo 1
    // analogInput.addPot(POT_PIN_2, 2);   // Pot on GPIO32 -> Servo 2
    // analogInput.addPot(POT_PIN_3, 3);   // Pot on GPIO33 -> Servo 3
    // analogInput.addJoystick(JOY1_X_PIN, JOY1_Y_PIN, 4, 5); // Joystick -> Servo 4,5

    Serial.println("=== Servo Controller Ready ===\n");
}

void loop() {
    // Read analog inputs and update servos
    analogInput.update();

    // Advance sequence playback / recording
    sequenceRecorder.update();

    // Smooth servo movement (if smoothing enabled)
    servoController.update();

    // Send BLE state notifications periodically (every 200ms)
    unsigned long now = millis();
    if (now - lastBleNotify >= 200) {
        lastBleNotify = now;
        bleController.updateStateNotification();
    }

    // WiFi + BLE handled via callbacks/async - no polling needed
}
