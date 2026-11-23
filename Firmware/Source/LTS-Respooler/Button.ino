// ---------------------------- Button -----------------------------

// Press: start or pause. Hold for 1 s while paused or stopped: cancel
void handleButton(unsigned long now) {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastStableButtonState && now - lastDebounceTime > BUTTON_DEBOUNCE_MS) {
    lastDebounceTime = now;

    if (reading == LOW) {
      if (!isMotorRunning) {
        if (currentState == 'P' || currentState == 'A') {
          buttonPressSince = now;
          buttonLongPressHandled = false;
          buttonAwaitingDecision = true;
        } else if (useFilamentSensor && !filamentDetected) {
          if (!otaInProgress) {
            for (int i = 0; i < 5; i++) {
              led.setPixelColor(0, led.Color(0, 0, pwmValue));
              led.show();
              delay(100);
              led.setPixelColor(0, 0);
              led.show();
              delay(100);
            }
          }
        } else {
          shouldStartMotorNow = true;
          delayStartUntil = now;
        }
      } else {
        isMotorRunning = false;
        stepperStop();
        digitalWrite(EN_PIN, HIGH);
        digitalWrite(STEP_PIN, LOW);
        unsigned long elapsed = 0;
        if (spoolingStartTime > 0) {
          elapsed = millis() - spoolingStartTime;
        }
        pausedElapsed = elapsed;
        spoolingStartTime = 0;
        currentState = 'P';
        sendStatus(true);
      }
    } else {
      if (buttonAwaitingDecision) {
        if (!buttonLongPressHandled) {
          if (!(useFilamentSensor && !filamentDetected)) {
            shouldStartMotorNow = true;
            delayStartUntil = now;
          }
        }
        buttonAwaitingDecision = false;
      }
    }
    lastStableButtonState = reading;
  }
  if (buttonAwaitingDecision && !buttonLongPressHandled && (digitalRead(BUTTON_PIN) == LOW)) {
    if (now - buttonPressSince >= 1000) {
      remHoldActive = true;
      remHoldExpiry = millis() + 200;
      progHoldActive = true;
      progHoldExpiry = millis() + 200;
      isMotorRunning = false;
      progress = 0.0;
      totalEstimatedTime = 0;
      spoolingStartTime = 0;
      pausedElapsed = 0;
      stepperStop();
      digitalWrite(EN_PIN, HIGH);
      digitalWrite(STEP_PIN, LOW);
      currentState = 'I';
      sendStatus(true);
      buttonLongPressHandled = true;
    }
  }
}
