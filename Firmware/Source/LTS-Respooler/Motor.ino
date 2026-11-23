// ----------------------------- Motor -----------------------------

void applySpeedTarget() {
  if (highSpeedMode) {
    unsigned long adjusted = (unsigned long)round((double)baseTargetStepIntervalMicros / HIGH_SPEED_FACTOR);
    if (adjusted < 50) adjusted = 50;
    targetStepIntervalMicros = adjusted;
  } else {
    targetStepIntervalMicros = baseTargetStepIntervalMicros;
  }
}

void setMotorCurrent(int percent) {
  int strengthCurrent = map(percent, 80, 120, MOTOR_CURRENT_MIN_MA, MOTOR_CURRENT_MAX_MA);
  driver.rms_current(strengthCurrent);
}

void reSanitizeDriver() {
  driver.GSTAT(0b111);
  driver.I_scale_analog(false);
  driver.en_spreadCycle(HIGH_SPEED_SPREADCYCLE && highSpeedMode);
  driver.pwm_autoscale(true);
  driver.pwm_autograd(true);
  driver.microsteps(MICROSTEPPING);
  setMotorCurrent(motorStrength);
}

// Debounced filament sensor, a paused or stopped respool ends when the filament is gone
void updateFilamentSensor(unsigned long now) {
  bool rawFilamentPresent = digitalRead(FILAMENT_PIN) == LOW;
  if (rawFilamentPresent) {
    filamentLostSince = 0;
    filamentDetected = true;
  } else {
    if (filamentLostSince == 0) {
      filamentLostSince = now;
    }
    if (filamentDetected && (now - filamentLostSince >= filamentLossConfirmMs())) {
      filamentDetected = false;
    }
  }

  if (useFilamentSensor && (currentState == 'P' || currentState == 'A') && !filamentDetected) {
    isMotorRunning = false;
    shouldStartMotorNow = false;
    pendingDirectionChange = false;
    triggerJingleNow = 0;
    remHoldActive = true;
    remHoldExpiry = millis() + 200;
    progHoldActive = true;
    progHoldExpiry = millis() + 200;
    progress = 0.0;
    totalEstimatedTime = 0;
    spoolingStartTime = 0;
    pausedElapsed = 0;
    stepperStop();
    digitalWrite(EN_PIN, HIGH);
    digitalWrite(STEP_PIN, LOW);
    currentState = 'I';
    sendStatus(true);
  }
}

// Stops with an alarm when the StallGuard value stays below the torque limit
void checkTorqueStop(unsigned long now) {
  static unsigned long lastTorqueCheck = 0;
  static unsigned long torqueBelowStartTime = 0;
  if (torqueLimit > 0 && isMotorRunning && stepIntervalMicros == targetStepIntervalMicros) {
    if (now - lastTorqueCheck >= TORQUE_CHECK_INTERVAL_MS) {
      lastTorqueCheck = now;
      uint16_t sg = driver.SG_RESULT();
      if (sg > TORQUE_SG_IGNORE) {

        float strengthScale = 1.0;
        int baseLimit = 350 * strengthScale;
        int limit = 999;
        if (torqueLimit == 1) limit = baseLimit * TORQUE_SG_LIMIT_LOW;
        else if (torqueLimit == 2) limit = baseLimit * TORQUE_SG_LIMIT_MED;
        else if (torqueLimit == 3) limit = baseLimit * TORQUE_SG_LIMIT_HIGH;

        int currSpeedPercent = getSpeedPercentFromInterval(stepIntervalMicros);
        if (currSpeedPercent > 94) {
          limit = limit * 0.95;
        } else if (currSpeedPercent > 85) {
          limit = limit * 0.97;
        } else if (currSpeedPercent < 65) {
          limit = limit * 0.98;
        }

        if (sg < limit) {
          if (torqueBelowStartTime == 0) torqueBelowStartTime = now;
          else if (now - torqueBelowStartTime >= TORQUE_BELOW_MS) {
            isMotorRunning = false;
            currentState = 'A';
            sendStatus(true);
            digitalWrite(STEP_PIN, LOW);
            delay(100);
            digitalWrite(EN_PIN, HIGH);
            unsigned long elapsed = millis() - spoolingStartTime;
            spoolingStartTime = 0;
            pausedElapsed = elapsed;
            torqueBelowStartTime = 0;
            stepperStop();
            ledc_stop(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL, 0);
            gpio_reset_pin((gpio_num_t)STEP_PIN);
            gpio_set_direction((gpio_num_t)STEP_PIN, GPIO_MODE_OUTPUT);
            digitalWrite(EN_PIN, LOW);
            digitalWrite(STEP_PIN, LOW);
            delay(120);
            {
              led.setPixelColor(0, led.Color(pwmValue, 0, 0));
              led.show();
            }
            for (int i = 0; i < 3; i++) {
              playTone(850, 250);
              delay(100);
            }
            digitalWrite(EN_PIN, HIGH);
            {
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
            }
            stepperStop();
          }
        } else {
          torqueBelowStartTime = 0;
        }
      }
    }
  } else {
    torqueBelowStartTime = 0;
  }
}

// Done once the selected spool weight is reached
void checkTargetWeight(unsigned long now) {
  static unsigned long wgtFinishStart = 0;
  if (isMotorRunning && targetWeight >= 1 && targetWeight <= 3 && totalEstimatedTime > 0) {
    unsigned long elapsed = 0;
    if (spoolingStartTime > 0) elapsed = now - spoolingStartTime;
    else if (pausedElapsed > 0) elapsed = pausedElapsed;
    else if (currentState == 'A') elapsed = pausedElapsed;

    unsigned long effectiveTotal = totalEstimatedTime;
    if (targetWeight == 2) effectiveTotal = (unsigned long)(totalEstimatedTime * TARGET_WEIGHT_FACTOR_2);
    else if (targetWeight == 3) effectiveTotal = (unsigned long)(totalEstimatedTime * TARGET_WEIGHT_FACTOR_3);

    if (effectiveTotal == 0) {
      wgtFinishStart = 0;
    } else if (elapsed >= effectiveTotal) {
      if (!(useFilamentSensor && !filamentDetected)) {
        if (wgtFinishStart == 0) wgtFinishStart = now;
        if (now - wgtFinishStart >= 700) {
          isMotorRunning = false;
          digitalWrite(STEP_PIN, LOW);
          stepperStop();
          digitalWrite(EN_PIN, HIGH);
          totalEstimatedTime = 0;
          spoolingStartTime = 0;
          currentState = 'D';
          sendStatus(true);
          progress = 100.0f;
          sendStatus(true);
          doneHoldActive = true;
          doneHoldStart = millis();
          playStepperJingle();
          wgtFinishStart = 0;
        }
      } else {
        wgtFinishStart = 0;
      }
    } else {
      wgtFinishStart = 0;
    }
  }
}

// Done when the filament runs out, back to idle 20 s later
void checkFilamentEnd(unsigned long now) {
  if (isMotorRunning && useFilamentSensor && !filamentDetected && filamentLostSince > 0 && now - filamentLostSince > filamentLossConfirmMs()) {
    isMotorRunning = false;
    digitalWrite(STEP_PIN, LOW);
    stepperStop();
    delay(40);
    digitalWrite(EN_PIN, HIGH);
    totalEstimatedTime = 0;
    spoolingStartTime = 0;
    currentState = 'D';
    sendStatus(true);
    progress = 100.0f;
    sendStatus(true);
    doneHoldActive = true;
    doneHoldStart = millis();
    delay(300);
    playStepperJingle();
  }
  if (doneHoldActive && (millis() - doneHoldStart >= 20000)) {
    progress = 0.0;
    currentState = 'I';
    sendStatus(true);
    doneHoldActive = false;
  }
}

// A direction change while running stops, turns and starts again
void applyPendingDirection(unsigned long now) {
  if (pendingDirectionChange && !isMotorRunning) {
    motorDirection = newMotorDirection;
    prefs.begin("respooler", false);
    prefs.putUInt("dir", motorDirection);
    prefs.end();
    digitalWrite(DIR_PIN, motorDirection);
    delayStartUntil = now + 1000;
    shouldStartMotorNow = true;
    pendingDirectionChange = false;
  }
}

// False ends this loop pass, as during an OTA update
bool startMotorIfRequested(unsigned long now) {
  if (shouldStartMotorNow && now >= delayStartUntil) {
    if (otaInProgress) {
      shouldStartMotorNow = false;
      return false;
    }
    if (isMotorRunning) {
      shouldStartMotorNow = false;
    } else if (!useFilamentSensor || filamentDetected) {
      totalEstimatedTime = estimatedTotalMs(targetStepIntervalMicros);
      if (pausedElapsed > 0) {
        spoolingStartTime = millis() - pausedElapsed;
        pausedElapsed = 0;
      } else {
        spoolingStartTime = millis();
      }
      currentState = 'R';
      sendStatus(true);
      remStartHoldUntil = millis() + 800;
      doneHoldActive = false;
      stepperStop();
      reSanitizeDriver();
      digitalWrite(DIR_PIN, motorDirection);
      digitalWrite(EN_PIN, LOW);
      digitalWrite(STEP_PIN, LOW);
      delay(15);
      {
        uint16_t prevI = driver.rms_current();
        driver.en_spreadCycle(true);
        int boosted = prevI + prevI / 4;
        if (boosted > 1600) boosted = 1600;
        driver.rms_current(boosted);
        stepperStart(stepFreqFromIntervalUs(1800));
        delay(150);
        driver.rms_current(prevI);
        driver.en_spreadCycle(HIGH_SPEED_SPREADCYCLE && highSpeedMode);
      }
      stepIntervalMicros = START_INTERVAL_US;
      lastAccelUpdate = now;
      isMotorRunning = true;
      stepperStart(stepFreqFromIntervalUs(stepIntervalMicros));
    }
    shouldStartMotorNow = false;
  }
  return true;
}

// Speed ramp, keeps the progress when the target speed changes
void updateMotorRamp(unsigned long now) {
  if (isMotorRunning && now - lastAccelUpdate > ACCEL_UPDATE_INTERVAL) {
    lastAccelUpdate = now;
    if (stepIntervalMicros > targetStepIntervalMicros) {
      stepIntervalMicros -= ACCEL_STEP;
      if (stepIntervalMicros < targetStepIntervalMicros) stepIntervalMicros = targetStepIntervalMicros;
    } else if (stepIntervalMicros < targetStepIntervalMicros) {
      stepIntervalMicros += ACCEL_STEP;
      if (stepIntervalMicros > targetStepIntervalMicros) stepIntervalMicros = targetStepIntervalMicros;
    }
    if (stepIntervalMicros != lastIntervalMicros) {
      stepperSetFreq(stepFreqFromIntervalUs(stepIntervalMicros));
      lastIntervalMicros = stepIntervalMicros;
    }
    if (stepIntervalMicros == targetStepIntervalMicros) {
      unsigned long newTotal = estimatedTotalMs(stepIntervalMicros);
      if (totalEstimatedTime > 0 && newTotal != totalEstimatedTime) {
        unsigned long elapsed = millis() - spoolingStartTime;
        double progressRatio = (double)elapsed / totalEstimatedTime;
        spoolingStartTime = millis() - (unsigned long)(newTotal * progressRatio);
      }
      totalEstimatedTime = newTotal;
    }
  }
}

void updateFan() {
  int fanPWM = (fanAlwaysOn || isMotorRunning || millis() < fanStopAfter) ? fanPWMFromSpeed(fanSpeed) : 0;
  setLedcDuty(FAN_CHANNEL, fanPWM);
  if (!isMotorRunning && fanPWM > 0 && fanStopAfter == 0) {
    fanStopAfter = millis() + 10000;
  }
  if (isMotorRunning) fanStopAfter = millis() + 10000;
  if (!fanAlwaysOn && !isMotorRunning && millis() >= fanStopAfter) {
    fanStopAfter = 0;
  }
}

// ------------------- Stepper jingle/tones --------------------------
void playTone(unsigned int freqHz, unsigned long durationMs) {
  uint16_t prevCurrent = driver.rms_current();
  driver.rms_current(1400);
  unsigned long halfPeriod = 1000000UL / (2 * freqHz);
  unsigned long totalCycles = (durationMs * 1000UL) / (2 * halfPeriod);
  bool dirState = false;
  for (unsigned long i = 0; i < totalCycles; i++) {
    dirState = !dirState;
    digitalWrite(DIR_PIN, dirState);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(40);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(halfPeriod * 2 - 40);
  }
  driver.rms_current(prevCurrent);
}
void playStepperJingle() {
  if (jingleStyle == 0) return;
  delay(500);
  stepperStop();
  ledc_stop(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL, 0);
  gpio_reset_pin((gpio_num_t)STEP_PIN);
  gpio_set_direction((gpio_num_t)STEP_PIN, GPIO_MODE_OUTPUT);
  digitalWrite(EN_PIN, LOW);
  digitalWrite(STEP_PIN, LOW);
  {
    led.setPixelColor(0, led.Color(0, pwmValue, 0));
    led.show();
  }
  switch (jingleStyle) {
    case 1:  // Simple
      playTone(1000, 140);
      delay(20);
      playTone(800, 140);
      delay(20);
      playTone(1200, 400);
      break;
    case 2:  // Glissando
      playTone(523, 220);
      delay(20);
      playTone(587, 70);
      delay(20);
      playTone(659, 70);
      delay(20);
      playTone(698, 70);
      delay(20);
      playTone(784, 70);
      delay(20);
      playTone(880, 70);
      delay(20);
      playTone(988, 100);
      delay(20);
      playTone(1047, 400);
      break;
    case 3:  // Star Wars
      playTone(440, 400);
      delay(40);
      playTone(440, 400);
      delay(40);
      playTone(440, 400);
      delay(40);
      playTone(349, 250);
      delay(40);
      playTone(523, 150);
      delay(40);
      playTone(440, 400);
      delay(40);
      playTone(349, 250);
      delay(40);
      playTone(523, 150);
      delay(40);
      playTone(440, 500);
      break;
  }
  digitalWrite(EN_PIN, HIGH);
  {
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
  }
  stepperStop();
}
