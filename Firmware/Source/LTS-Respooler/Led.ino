// ----------------------------- LEDs ------------------------------

// Blue pulse on the status LED during an OTA update
void otaLedPulseTask(void* parameter) {
  pwmValue = calculatePWM(ledBrightness);
  int otaLedPulse = 0;
  int otaLedDir = 1;
  uint16_t blueDitherAcc = 0;

  while (otaInProgress) {
    otaLedPulse += otaLedDir * 1;
    if (otaLedPulse >= LED_CONN_PULSE_MAX) {
      otaLedPulse = LED_CONN_PULSE_MAX;
      otaLedDir = -1;
    } else if (otaLedPulse <= 0) {
      otaLedPulse = 0;
      otaLedDir = 1;
    }

#if CONTROL_BOARD
    setConnLed((uint8_t)otaLedPulse);
#else
    setLedcDuty(LEDC_CHANNEL_2, LED_CONN_PULSE_MAX - otaLedPulse);
#endif

    uint16_t pulse255 = (uint16_t)((otaLedPulse * 255 + (LED_CONN_PULSE_MAX / 2)) / LED_CONN_PULSE_MAX);
    uint16_t scaled16 = (uint16_t)pulse255 * (uint16_t)pwmValue;
    uint8_t baseVal = scaled16 / 255;
    uint8_t remainder = scaled16 % 255;
    blueDitherAcc = (uint16_t)(blueDitherAcc + remainder);
    if (blueDitherAcc >= 255) {
      baseVal++;
      blueDitherAcc -= 255;
    }

    led.setPixelColor(0, led.Color(0, 0, baseVal));
    led.show();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
#if CONTROL_BOARD
  setConnLed(LED_CONN_PULSE_MAX);
#else
  setLedcDuty(LEDC_CHANNEL_2, LED_CONN_PULSE_MAX);
#endif
  led.setPixelColor(0, 0);
  led.show();
  vTaskDelete(NULL);
}

// Status LED: green running, orange paused, red blinking after an auto stop
void updateStatusLed(unsigned long now) {
  if (!otaInProgress) {
    if (isMotorRunning) {
      if (now - lastLedPulseTime > 5) {
        lastLedPulseTime = now;
        ledPulseValue += ledPulseDirection * 1;
        if (ledPulseValue >= 255) {
          ledPulseValue = 255;
          ledPulseDirection = -1;
        } else if (ledPulseValue <= 0) {
          ledPulseValue = 0;
          ledPulseDirection = 1;
        }
        uint16_t scaled16 = (uint16_t)ledPulseValue * (uint16_t)pwmValue;
        uint8_t baseVal = scaled16 / 255;
        uint8_t remainder = scaled16 % 255;
        greenDitherAcc = (uint16_t)(greenDitherAcc + remainder);
        if (greenDitherAcc >= 255) {
          baseVal++;
          greenDitherAcc -= 255;
        }

        led.setPixelColor(0, led.Color(0, baseVal, 0));
        led.show();
      }
    } else if (currentState == 'P') {
      if (now - lastLedPulseTime > 5) {
        lastLedPulseTime = now;
        ledPulseValue += ledPulseDirection * 1;
        if (ledPulseValue >= 255) {
          ledPulseValue = 255;
          ledPulseDirection = -1;
        } else if (ledPulseValue <= 0) {
          ledPulseValue = 0;
          ledPulseDirection = 1;
        }
        uint16_t scaled16 = (uint16_t)ledPulseValue * (uint16_t)pwmValue;
        uint8_t baseVal = scaled16 / 255;
        uint8_t remainder = scaled16 % 255;
        greenDitherAcc = (uint16_t)(greenDitherAcc + remainder);
        if (greenDitherAcc >= 255) {
          baseVal++;
          greenDitherAcc -= 255;
        }
        led.setPixelColor(0, led.Color(baseVal, baseVal / 2, 0));
        led.show();
      }
    } else if (currentState == 'A') {
      if (millis() - lastLedToggle > 400) {
        lastLedToggle = now;
        ledState = !ledState;
        led.setPixelColor(0, ledState ? led.Color(pwmValue, 0, 0) : 0);
        led.show();
      }
    } else if (useFilamentSensor && filamentDetected) {
      led.setPixelColor(0, led.Color(0, pwmValue, 0));
      led.show();
    } else {
      led.setPixelColor(0, led.Color(0, 0, pwmValue));
      led.show();
    }
  }
}

// Connection LED pulses while no app is connected over BLE
void updateConnLed(unsigned long now) {
  if (!otaInProgress) {
    static int connLedPulse = 0;
    static int connLedDir = 2;

    if (!deviceConnected) {
      if (now - lastConnLedTime > 25) {
        lastConnLedTime = now;
        connLedPulse += connLedDir;
        if (connLedPulse >= LED_CONN_PULSE_MAX) {
          connLedPulse = LED_CONN_PULSE_MAX;
          connLedDir = -2;
        } else if (connLedPulse <= 0) {
          connLedPulse = 0;
          connLedDir = 2;
        }
#if CONTROL_BOARD
        setConnLed(connLedPulse);
#else
        setLedcDuty(LEDC_CHANNEL_2, LED_CONN_PULSE_MAX - connLedPulse);
#endif
      }
    } else {
      setLedcDuty(LEDC_CHANNEL_2, LED_CONN_PULSE_MAX);
    }
  }
}
