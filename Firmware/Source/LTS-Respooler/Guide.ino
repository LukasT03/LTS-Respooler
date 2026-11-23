// ------------------------- Filament guide ------------------------

// Moves the guide one step per spool turn while running, otherwise homes or goes to a side
bool updateGuideServo(unsigned long now) {
  if (isMotorRunning) {
    if (!servoWasRunning) {
      servoWasRunning = true;
      lastMotorStepAccTime = now;
      servoHomingToHome = false;
      servoGotoActive = false;
    } else {
      unsigned long dtMs = now - lastMotorStepAccTime;
      if (dtMs > 0) {
        lastMotorStepAccTime = now;

        float stepsPerSec = (float)stepFreqFromIntervalUs(stepIntervalMicros);
        motorStepAcc += stepsPerSec * ((float)dtMs / 1000.0f);
        float stepsPerSpoolRev = (200.0f * (float)MICROSTEPPING) * (52.0f / 18.0f);

        while (motorStepAcc >= stepsPerSpoolRev) {
          motorStepAcc -= stepsPerSpoolRev;

          float target = guideServoTowardsL ? (float)servoAngleL : (float)servoAngleR;
          float stepDeg = servoStepMm * (360.0f / (35.0f * 3.77f));
          if (stepDeg < 0.05f) stepDeg = 0.05f;

          if (fabsf(guideServoPos - target) <= stepDeg) {
            guideServoPos = target;
            setServoAngle((int)lroundf(guideServoPos));
            guideServoTowardsL = !guideServoTowardsL;
          } else {
            guideServoPos += (guideServoPos < target) ? stepDeg : -stepDeg;
            setServoAngle((int)lroundf(guideServoPos));
          }
        }
      }
    }
  } else {
    servoWasRunning = false;
  }

  if (currentState == 'I' && lastStateForServo != 'I') {
    guideServoTowardsL = servoHomeIsR;
    motorStepAcc = 0.0f;
    servoWasRunning = false;
    servoHomingToHome = true;
    lastServoHomeStepTime = now;
  }
  lastStateForServo = currentState;
  
  // ------------------ Respooler done ------------------
  if (currentState == 'D' && lastStateForDone != 'D') {
    servoHomeAfterDonePending = true;
  }
  if (currentState != 'D' && lastStateForDone == 'D') {
    servoHomeAfterDonePending = false;
  }
  lastStateForDone = currentState;

  if (servoHomeAfterDonePending && currentState == 'D' && !isMotorRunning) {
    guideServoTowardsL = servoHomeIsR;
    servoHomingToHome = true;
    lastServoHomeStepTime = now;
    servoHomeAfterDonePending = false;
  }

  // ------------------ Servo goto ------------------
  if (!isMotorRunning && servoGotoActive) {
    if (now - lastServoHomeStepTime >= 10) {
      lastServoHomeStepTime = now;

      float target;
      if (servoGotoSide == 'L') target = (float)servoAngleL;
      else if (servoGotoSide == 'R') target = (float)servoAngleR;
      else if (servoGotoSide == 'H') target = (float)(servoHomeIsR ? servoAngleR : servoAngleL);
      else {
        servoGotoActive = false;
        return false;
      }

      float stepDeg = 1.0f;

      if (fabsf(guideServoPos - target) <= stepDeg) {
        guideServoPos = target;
        setServoAngle((int)lroundf(guideServoPos));
        servoGotoActive = false;
      } else {
        guideServoPos += (guideServoPos < target) ? stepDeg : -stepDeg;
        setServoAngle((int)lroundf(guideServoPos));
      }
    }
  }

  // ------------------ Servo homing ------------------
  if (!isMotorRunning && servoHomingToHome) {
    if (now - lastServoHomeStepTime >= 10) {
      lastServoHomeStepTime = now;

      float targetHome = (float)(servoHomeIsR ? servoAngleR : servoAngleL);
      float stepDeg = 1.0f;

      if (fabsf(guideServoPos - targetHome) <= stepDeg) {
        guideServoPos = targetHome;
        setServoAngle((int)lroundf(guideServoPos));
        servoHomingToHome = false;
      } else {
        guideServoPos += (guideServoPos < targetHome) ? stepDeg : -stepDeg;
        setServoAngle((int)lroundf(guideServoPos));
      }
    }
  }
  return true;
}
