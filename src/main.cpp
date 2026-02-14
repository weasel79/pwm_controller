#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"
#include "output_controller.h"
#include "wifi_controller.h"
#include "ps4_input.h"
#include "analog_input.h"
#include "sequence_recorder.h"

OutputController  outputController;
WiFiController   wifiController;
PS4Input         ps4Input;
AnalogInput      analogInput;
SequenceRecorder sequenceRecorder;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Output Controller Starting ===");

    // Initialize LittleFS for sequence storage
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed!");
    } else {
        Serial.println("[FS] LittleFS mounted");
    }

    // Initialize output controller (PCA9685)
    outputController.init();

    // Initialize sequence recorder
    sequenceRecorder.init(&outputController);

    // Initialize WiFi + Web UI + REST API
    wifiController.init(&outputController, &sequenceRecorder);

    // Initialize PS4 controller (Bluetooth Classic)
    ps4Input.init(&outputController);

    // Initialize analog inputs
    analogInput.init(&outputController);

    // Configure default pot/joystick mappings
    // Uncomment and adjust pins/channels for your wiring:
     analogInput.addPot(POT_PIN_0, 0);   // Pot on GPIO34 -> Output 0
    // analogInput.addPot(POT_PIN_1, 1);   // Pot on GPIO35 -> Output 1
    // analogInput.addPot(POT_PIN_2, 2);   // Pot on GPIO32 -> Output 2
    // analogInput.addPot(POT_PIN_3, 3);   // Pot on GPIO33 -> Output 3
    // analogInput.addJoystick(JOY1_X_PIN, JOY1_Y_PIN, 4, 5); // Joystick -> Output 4,5

    Serial.printf("[Heap] Free: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.println("=== Output Controller Ready ===\n");
}

void loop() {
    // Read PS4 controller inputs
    ps4Input.update();

    // Read analog inputs and update outputs
    analogInput.update();

    // Advance sequence playback / recording
    sequenceRecorder.update();

    // Smooth output movement (if smoothing enabled)
    outputController.update();
}
