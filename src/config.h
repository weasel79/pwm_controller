#pragma once

// ---- WiFi Configuration ----
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define WIFI_AP_SSID    "ServoController"
#define WIFI_AP_PASSWORD "servo1234"
// Set to true to connect to an existing network (STA mode),
// false to create an access point (AP mode)
#define WIFI_STA_MODE   false

// ---- I2C Pins (PCA9685) ----
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define PCA9685_ADDR    0x40

// ---- Servo Configuration ----
#define NUM_SERVOS          16
#define SERVO_FREQ_HZ       50
#define SERVO_MIN_US        500
#define SERVO_MAX_US       2500
#define SERVO_DEFAULT_ANGLE  90

// ---- Analog Input Pins (ADC1 only - ADC2 conflicts with WiFi) ----
// Potentiometer pins (connect wiper to pin, ends to 3.3V and GND)
#define POT_PIN_0       34
#define POT_PIN_1       35
#define POT_PIN_2       32
#define POT_PIN_3       33

// Joystick pins
#define JOY1_X_PIN      36
#define JOY1_Y_PIN      39

// ---- Analog Input Settings ----
#define ADC_RESOLUTION      12          // 12-bit ADC (0-4095)
#define ANALOG_DEADZONE     30          // Ignore changes smaller than this (out of 4095)
#define ANALOG_SMOOTHING    0.15f       // EMA alpha (0.0 = no change, 1.0 = no smoothing)
#define ANALOG_UPDATE_MS    20          // Read interval in ms

// ---- Sequence Recorder ----
#define SEQUENCE_DIR        "/sequences"
#define SEQUENCE_FRAME_MS   50          // Capture interval (20 fps)
#define SEQUENCE_MAX_FRAMES 2000        // Max frames per sequence (~100s at 20fps)

// ---- BLE ----
#define BLE_DEVICE_NAME     "ServoCtrl"
#define BLE_SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_SERVO_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_BULK_CHAR_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define BLE_STATE_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// ---- Web Server ----
#define WEB_SERVER_PORT     80
