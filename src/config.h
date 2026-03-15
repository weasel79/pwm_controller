#pragma once

// ---- WiFi Configuration ----
#define WIFI_SSID       "blndr"
#define WIFI_PASSWORD   "1Secret3"
#define WIFI_AP_SSID    "UniversalPWM"
#define WIFI_AP_PASSWORD "servo1234"
// Set to true to connect to an existing network (STA mode),
// false to create an access point (AP mode)
#define WIFI_STA_MODE   true

// ---- I2C Pins (PCA9685) ----
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define PCA9685_ADDR    0x40

// ---- Output Configuration ----
#define NUM_OUTPUTS          16
#define OUTPUT_FREQ_HZ       50
#define OUTPUT_DEFAULT_MIN_US  500
#define OUTPUT_DEFAULT_MAX_US 2500
#define OUTPUT_DEFAULT_VALUE   90

// PWM tick defaults (12-bit PCA9685: 0-4095)
// Servo @ 50Hz: 500us = 102 ticks, 2500us = 512 ticks  (ticks = us * 4096 / 20000)
#define SERVO_DEFAULT_PWM_MIN  102
#define SERVO_DEFAULT_PWM_MAX  512
#define MOTOR_DEFAULT_PWM_MIN  0
#define MOTOR_DEFAULT_PWM_MAX  4095

// ---- Analog Input Pins (ADC1 only - ADC2 conflicts with WiFi) ----
// Potentiometer pins (connect wiper to pin, ends to 3.3V and GND)
#define NUM_POTS        4
static const uint8_t POT_PINS[NUM_POTS] = {34, 35, 32, 33};

// ---- Analog Input Settings ----
#define ADC_RESOLUTION      12          // 12-bit ADC (0-4095)
#define ANALOG_DEADZONE     30          // Ignore changes smaller than this (out of 4095)
#define ANALOG_SMOOTHING    0.15f       // EMA alpha (0.0 = no change, 1.0 = no smoothing)
#define ANALOG_UPDATE_MS    20          // Read interval in ms

// ---- Sequence Data (per-channel) ----
#define SEQDATA_DIR         "/seqdata"

// ---- PS4 Controller ----
// MAC address for Bluetooth pairing (set controller to match via SixAxis Pair Tool)
#define PS4_CONTROLLER_MAC  "1a:2b:3c:01:01:01"
#define PS4_UPDATE_MS       20

// ---- MouldKing BLE Motor ----
#define NUM_MK_CHANNELS  6          // MK 6.0: channels A-F
#define MK_CONNECT_MS    3000       // BLE connect broadcast duration
#define MK_UPDATE_MS     50         // Min interval between BLE motor updates

// ---- Logging ----
// 0=ERROR, 1=INFO, 2=DEBUG (verbose traces)
#define LOG_LEVEL           1

// ---- Web Server ----
#define WEB_SERVER_PORT     80
