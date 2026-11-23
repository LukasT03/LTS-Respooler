// ---------------------------- Board ------------------------------
// Pins and differences of the two boards. Which one is built follows from the board
// selected in the Arduino IDE: ESP32-S3 is the LTS Control Board (V4), ESP32 the
// Driver Board (ESP32-WROOM-32).

#pragma once

#if CONFIG_IDF_TARGET_ESP32S3
#define CONTROL_BOARD 1
#else
#define CONTROL_BOARD 0
#endif

#if CONTROL_BOARD

#define BOARD_NAME "LTS CB"
#define OTA_URL "https://respooler.lts-design.com/Firmware/ControlBoard_V4_OTA.bin"

#define TMC_UART_RX 18
#define TMC_UART_TX 17
#define STEP_PIN 14
#define DIR_PIN 21
#define EN_PIN 40
#define LED_PIN 9
#define LED_CONN_PIN 47
#define FILAMENT_PIN 11
#define BUTTON_PIN 10
#define FAN_PIN 13
#define SERVO_PIN 12
#define VBUS_SENSE_PIN 4
#define PD_GOOD_PIN 7

// USB-C power
#define VBUS_DIVIDER ((20.0f + 2.7f) / 2.7f)

// Motor current at 80 and 120 % motor strength
#define MOTOR_CURRENT_MIN_MA 700
#define MOTOR_CURRENT_MAX_MA 1400

// SpreadCycle instead of StealthChop in high-speed mode
#define HIGH_SPEED_SPREADCYCLE 1

#else

#define BOARD_NAME "LTS DB"
#define OTA_URL "https://respooler.lts-design.com/Firmware/ESP32-WROOM-32_OTA.bin"

#define TMC_UART_RX 21
#define TMC_UART_TX 22
#define STEP_PIN 14
#define DIR_PIN 27
#define EN_PIN 13
#define LED_PIN 19
#define LED_CONN_PIN 2
#define FILAMENT_PIN 33
#define BUTTON_PIN 25
#define FAN_PIN 32
#define SERVO_PIN 26

#define MOTOR_CURRENT_MIN_MA 900
#define MOTOR_CURRENT_MAX_MA 1600

#define HIGH_SPEED_SPREADCYCLE 0

#endif
