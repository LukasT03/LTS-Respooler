// ---------------------- LTS RESPOOLER FIRMWARE ----------------------
// One sketch for the LTS Control Board and the Driver Board, see Board.h.

#include <TMCStepper.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <math.h>
#include <string>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "esp_bt.h"
#include "esp_coexist.h"
#include <driver/ledc.h>
#include "esp_private/periph_ctrl.h"
#include "driver/gpio.h"
#include <freertos/semphr.h>
#include <ESPmDNS.h>
#include <NetworkClientSecure.h>
#include <esp_http_server.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include "Board.h"

// ------------------------ Firmware ------------------------------
#define FIRMWARE_VERSION "1.3.3"

// ------------------------ Motor Parameters ----------------------
#define MICROSTEPPING 8
#define R_SENSE 0.11f
#define HIGH_SPEED_FACTOR 1.4

// -------------------- Timing and Limits --------------------------
#define START_INTERVAL_US 700
#define DEFAULT_INTERVAL_US 116
#define TORQUE_CHECK_INTERVAL_MS 50
#define BUTTON_DEBOUNCE_MS 50
#define ACCEL_UPDATE_INTERVAL 20
#define ACCEL_STEP 5
#define LED_CONN_PULSE_MAX 60
#define STATUS_NOTIFY_INTERVAL 400
#define LED_BLINK_INTERVAL 1000
#define TORQUE_SG_IGNORE 20
#define TORQUE_SG_LIMIT_LOW 0.72
#define TORQUE_SG_LIMIT_MED 0.85
#define TORQUE_SG_LIMIT_HIGH 0.97
#define TORQUE_BELOW_MS 700
#define MIN_RSSI_THRESHOLD -90
#define FILAMENT_LOSS_REF_INTERVAL_US 93
#define DEFAULT_CALIBRATION_MS 840000
#define RESUME_SAVE_INTERVAL_MS 2000
#define STEP_DUTY_ON 512

#define TARGET_WEIGHT_FACTOR_2 0.59f
#define TARGET_WEIGHT_FACTOR_3 0.34f

#define FAN_CHANNEL LEDC_CHANNEL_1
#define FAN_TIMER LEDC_TIMER_1

#define STEP_LEDC_CHANNEL LEDC_CHANNEL_3
#define STEP_LEDC_TIMER LEDC_TIMER_2

#define SERVO_LEDC_CHANNEL LEDC_CHANNEL_0
#define SERVO_LEDC_TIMER LEDC_TIMER_3

// ------------------- BLE UUIDs -----------------------------------
#define SERVICE_UUID "9E05D06D-68A7-4E1F-A503-AE26713AC101"
#define CHARACTERISTIC_UUID "7CB2F1B4-7E3F-43D2-8C92-DF58C9A7B1A8"

// ------------------- Direct settings variables -------------------
int speedPercent = 85;                        // 50-100
int motorDirection = 0;                       // 0, 1
int ledBrightness = 50;                       // 0-100
bool useFilamentSensor = true;                // true, false
int filamentLossMs = 100;                     // 100, 200, 400
bool resumeAfterPowerLoss = false;            // continue paused after a power loss
int motorStrength = 100;                      // 80-120
int targetWeight = 0;                         // 0-3
int torqueLimit = 0;                          // 0-3
int jingleStyle = 0;                          // 0-3
unsigned long calibrationAt85Speed = DEFAULT_CALIBRATION_MS;  // ms
bool remoteAccess = false;                                    // true, false: connect to LTS Cloud
String boardName = "";                                        // string, up to 64 bytes

String wifiSSID = "";      // string
String wifiPassword = "";  // string

int fanSpeed = 60;         // 0–100
bool fanAlwaysOn = false;  // true, false
unsigned long fanStopAfter = 0;

int servoAngleR = 5;        // deg
int servoAngleL = 175;      // deg
float servoStepMm = 1.75f;  // mm
bool servoHomeIsR = true;   // true, false

// ----------------- Board variant identifier -----------------
uint8_t boardVariant = 0;

// Where a command came from. Over the cloud, Wi-Fi, remote access and firmware updates
// can't be changed, a mistake there could only be fixed at the board.
enum CommandSource : uint8_t { SRC_BLE, SRC_LOCAL, SRC_CLOUD };

// --------------- High-Speed mode state -----------------
bool highSpeedMode = false;
int savedTorqueLimitHS = 0;
unsigned long baseTargetStepIntervalMicros = DEFAULT_INTERVAL_US;

// -------------- Status variables and objects -----------------
HardwareSerial TMCSerial(1);
TMC2209Stepper driver(&TMCSerial, R_SENSE, 0);
NimBLECharacteristic* pCharacteristic = nullptr;
Preferences prefs;
String serialRxLine;

float guideServoPos = 0.0f;
bool guideServoTowardsL = true;
float motorStepAcc = 0.0f;
unsigned long lastMotorStepAccTime = 0;
bool servoWasRunning = false;
char lastStateForServo = 'I';
bool servoHomingToHome = false;
unsigned long lastServoHomeStepTime = 0;
bool servoHomeAfterDonePending = false;
char lastStateForDone = 'I';
bool servoGotoActive = false;
char servoGotoSide = 0;

StaticJsonDocument<640> lastStatusDoc;

bool deviceConnected = false;
bool isMotorRunning = false;
bool shouldStartMotorNow = false;
bool filamentDetected = false;
bool pendingDirectionChange = false;
bool lastStableButtonState = HIGH;
bool ledState = false;
int newMotorDirection = 0;
int pwmValue = 0;
int triggerJingleNow = 0;
unsigned long delayStartUntil = 0;
unsigned long filamentLostSince = 0;
unsigned long stepIntervalMicros = START_INTERVAL_US;
unsigned long targetStepIntervalMicros = DEFAULT_INTERVAL_US;
unsigned long spoolingStartTime = 0;
unsigned long lastNotify = 0;
unsigned long lastLedToggle = 0;
unsigned long lastAccelUpdate = 0;
unsigned long lastConnLedTime = 0;
unsigned long lastDebounceTime = 0;
unsigned long lastIntervalMicros = 0;
unsigned long totalEstimatedTime = 0;
unsigned long pausedElapsed = 0;
unsigned long buttonPressSince = 0;
bool buttonLongPressHandled = false;
bool buttonAwaitingDecision = false;
bool doneHoldActive = false;
unsigned long doneHoldStart = 0;
bool remHoldActive = false;
unsigned long remHoldExpiry = 0;
bool progHoldActive = false;
unsigned long progHoldExpiry = 0;
unsigned long remStartHoldUntil = 0;
float progress = 0.0;
bool lastIsMotorRunning = false;
bool lastFilamentDetected = false;
float lastProgress = 0.0;
int lastSpeedPercent = 100;
int lastChipTemperature = 0;
int lastRemainingTime = 0;
volatile bool otaInProgress = false;
SemaphoreHandle_t bleNotifyMutex = nullptr;
char currentState = 'I';

Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);
int ledPulseValue = 0;
int ledPulseDirection = 1;
unsigned long lastLedPulseTime = 0;
uint16_t greenDitherAcc = 0;

// ------------------------ Task handles --------------------------
TaskHandle_t otaLedPulseTaskHandle = nullptr;
TaskHandle_t wifiScanTaskHandle = nullptr;

// -------------- Wi-Fi connect async status --------------
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartTime = 0;

// ------------------ Helper and utility functions ---------------
int calculatePWM(int brightnessPercent) {
  float gamma = 1.15f;
  float n = (float)constrain(brightnessPercent, 0, 100) / 100.0f;
  return (int)round(powf(n, gamma) * 255.0f);
}
int fanPWMFromSpeed(int percent) {
  return map(constrain(percent, 0, 100), 0, 100, 0, 255);
}

// Only write LEDC duty when it changes, frequent updates can hang the ESP32
uint32_t ledcDuty[LEDC_CHANNEL_MAX];
inline void setLedcDuty(ledc_channel_t channel, uint32_t duty) {
  if (ledcDuty[channel] == duty) return;
  ledcDuty[channel] = duty;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}
inline void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  int pulseUs = map(angle, 0, 180, 500, 2500);
  uint32_t duty = (uint32_t)((((uint64_t)pulseUs) * ((1UL << 14) - 1) + 10000UL) / 20000UL);
  setLedcDuty(SERVO_LEDC_CHANNEL, duty);
}
inline void setConnLed(uint8_t duty) {
  duty = constrain(duty, 0, 255);
  setLedcDuty(LEDC_CHANNEL_2, 255 - duty);
}
inline uint32_t stepFreqFromIntervalUs(unsigned long us) {
  if (us < 1) us = 1;
  return (uint32_t)(1000000UL / (2UL * us));
}
inline void stepperSetFreq(uint32_t freqHz) {
  if (freqHz < 1) freqHz = 1;
  ledc_set_freq(LEDC_LOW_SPEED_MODE, STEP_LEDC_TIMER, freqHz);
}
inline void stepperStart(uint32_t freqHz) {
  stepperSetFreq(freqHz);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL, STEP_DUTY_ON);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL);
}
inline void stepperStop() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL);
}
int getSpeedPercentFromInterval(unsigned long us) {
  double t = (double)us;
  double p = 50.0 + ((170.0 - t) * 50.0 / (170.0 - 93.0));
  int percent = (int)round(p);
  return constrain(percent, 50, 100);
}

// Scale with speed so the same length of filament is tolerated, never below the set value
unsigned long filamentLossConfirmMs() {
  if (targetStepIntervalMicros <= FILAMENT_LOSS_REF_INTERVAL_US) return filamentLossMs;
  return (unsigned long)filamentLossMs * targetStepIntervalMicros / FILAMENT_LOSS_REF_INTERVAL_US;
}

// Time for a full 1 kg spool, the calibration is done at 85 % speed
unsigned long estimatedTotalMs(unsigned long intervalUs) {
  int currSpeedPercent = round((float)speedPercent * ((float)baseTargetStepIntervalMicros / (float)intervalUs));
  currSpeedPercent = constrain(currSpeedPercent, 50, 140);
  return calibrationAt85Speed * (85.0 / currSpeedPercent);
}

void sendStatus(bool forceSend = false);

// -------------------------- Setup --------------------------------
void setup() {
  // LEDC is not reset by a software restart, so do it here
  periph_module_reset(PERIPH_LEDC_MODULE);
  memset(ledcDuty, 0xFF, sizeof(ledcDuty));

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  esp_coex_preference_set(ESP_COEX_PREFER_BT);
  pinMode(FILAMENT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_CONN_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
#if CONTROL_BOARD
  pinMode(PD_GOOD_PIN, INPUT);
#endif

  ledc_timer_config_t servo_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_14_BIT,
    .timer_num = SERVO_LEDC_TIMER,
    .freq_hz = 50,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&servo_timer);

  ledc_channel_config_t servo_channel = {
    .gpio_num = SERVO_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = SERVO_LEDC_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = SERVO_LEDC_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&servo_channel);

  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = FAN_TIMER,
    .freq_hz = 35000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  ledc_channel_config_t channel_conf = {
    .gpio_num = FAN_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = FAN_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = FAN_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);

  ledc_timer_config_t conn_led_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&conn_led_timer);

  ledc_channel_config_t conn_led_conf = {
    .gpio_num = LED_CONN_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_2,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&conn_led_conf);

  ledc_timer_config_t step_ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num = STEP_LEDC_TIMER,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&step_ledc_timer);

  ledc_channel_config_t step_ledc_chan = {
    .gpio_num = STEP_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = STEP_LEDC_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = STEP_LEDC_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&step_ledc_chan);

  led.begin();
  led.clear();
  led.show();

  Serial.begin(115200);
  TMCSerial.begin(115200, SERIAL_8N1, TMC_UART_RX, TMC_UART_TX);

  driver.begin();
  driver.I_scale_analog(false);
  driver.toff(3);
  driver.microsteps(MICROSTEPPING);
  driver.en_spreadCycle(false);
  driver.pwm_autoscale(true);
  driver.pwm_autograd(true);
  driver.SGTHRS(40);
  driver.semin(0);
  driver.TCOOLTHRS(0xFFFFF);
  uint16_t cool = driver.COOLCONF();
  driver.COOLCONF(cool | (1 << 13));

  prefs.begin("respooler", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pwd", "");
  prefs.end();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
  bleNotifyMutex = xSemaphoreCreateMutex();
  loadSettings();
  networkSetup();
  setMotorCurrent(motorStrength);
  pwmValue = calculatePWM(ledBrightness);

  guideServoPos = (float)(servoHomeIsR ? servoAngleR : servoAngleL);
  guideServoTowardsL = servoHomeIsR;
  setServoAngle((int)lroundf(guideServoPos));

  bleSetup();
  setLedcDuty(FAN_CHANNEL, 0);
  setConnLed(150);
  currentState = 'I';
  restoreResumeRecord();
}

// ------------------- Main loop -----------------------------------
void loop() {
  unsigned long now = millis();
  pollSerialCommands();
  networkPoll();
  checkWifiConnect();
  updateFilamentSensor(now);
  updateStatusLed(now);
  checkTorqueStop(now);
  checkTargetWeight(now);
  checkFilamentEnd(now);
  applyPendingDirection(now);
  if (!startMotorIfRequested(now)) return;
  handleButton(now);
  updateMotorRamp(now);
  updateConnLed(now);

  // ---------------------- Jingle trigger ---------------------
  if (triggerJingleNow != 0) {
    playStepperJingle();
    triggerJingleNow = 0;
  }

  restartAdvertisingIfIdle();
  updateResumeRecord(now);
  if (!updateGuideServo(now)) return;
  updateFan();

  // ----------- Periodic Status-Update every 500 ms -----------
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 500) {
    sendStatus();
    lastStatusTime = millis();
  }
}
