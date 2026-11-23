// --------------------------- Settings ---------------------------

void loadSettings() {
  prefs.begin("respooler", true);
  boardVariant = prefs.getUChar("variant", 0);
  baseTargetStepIntervalMicros = prefs.getUInt("speed", DEFAULT_INTERVAL_US);
  speedPercent = getSpeedPercentFromInterval(baseTargetStepIntervalMicros);
  applySpeedTarget();
  motorDirection = prefs.getUInt("dir", 0);
  ledBrightness = prefs.getUInt("led", 50);
  useFilamentSensor = prefs.getBool("filamentSensor", true);
  filamentLossMs = prefs.getUInt("filLossMs", 100);
  resumeAfterPowerLoss = prefs.getBool("resume", false);
  motorStrength = prefs.getUInt("motorStrength", 100);
  torqueLimit = prefs.getUInt("torqueLimit", 0);
  highSpeedMode = prefs.getBool("hsMode", false);
  if (highSpeedMode) {
    savedTorqueLimitHS = torqueLimit;
    torqueLimit = 0;
  }
  jingleStyle = prefs.getUInt("jingle", 0);
  // Firmware 1.3.2 and older stored the calibration at 80 % speed, convert it and round to 5 s
  unsigned long oldCalibration = prefs.getULong("cal80_093", 0);
  calibrationAt85Speed = prefs.getULong("cal85", oldCalibration ? (oldCalibration * 80 / 85 + 2500) / 5000 * 5000 : DEFAULT_CALIBRATION_MS);
  fanSpeed = prefs.getUInt("fanSpeed", 60);
  fanAlwaysOn = prefs.getBool("fanAlways", false);
  targetWeight = prefs.getUInt("targetWeight", 0);
  remoteAccess = prefs.getBool("remote", false);
  boardName = prefs.getString("name", "");

  servoAngleR = (int)prefs.getUInt("servoR", 5);
  servoAngleL = (int)prefs.getUInt("servoL", 175);
  servoStepMm = prefs.getFloat("servoStep", 1.75f);
  servoHomeIsR = prefs.getBool("servoHomeR", true);

  servoAngleR = constrain(servoAngleR, 0, 180);
  servoAngleL = constrain(servoAngleL, 0, 180);
  servoStepMm = constrain(servoStepMm, 0.05f, 20.0f);

  applySpeedTarget();
  prefs.end();
  setMotorCurrent(motorStrength);
  pwmValue = calculatePWM(ledBrightness);
  digitalWrite(DIR_PIN, motorDirection);
}

// ------------------- Resume after power loss -------------------
// Only saved while the motor runs, so a pause or stop is never restored
struct ResumeRecord {
  uint32_t elapsedMs;
  float servoPos;
  bool servoTowardsL;
};
// Own instance, the BLE task writes settings through prefs at the same time
Preferences resumePrefs;
bool resumeSaved = false;

void saveResumeRecord() {
  ResumeRecord rec = { millis() - spoolingStartTime, guideServoPos, guideServoTowardsL };
  resumePrefs.begin("resume", false);
  resumePrefs.putBytes("rec", &rec, sizeof(rec));
  resumePrefs.end();
  resumeSaved = true;
}

void clearResumeRecord() {
  resumePrefs.begin("resume", false);
  resumePrefs.clear();
  resumePrefs.end();
  resumeSaved = false;
}

// Continue paused with the saved progress and servo position, the motor never starts on its own
void restoreResumeRecord() {
  ResumeRecord rec;
  resumePrefs.begin("resume", true);
  resumeSaved = resumePrefs.getBytes("rec", &rec, sizeof(rec)) == sizeof(rec);
  resumePrefs.end();
  if (!resumeSaved || !resumeAfterPowerLoss) return;
  guideServoPos = rec.servoPos;
  guideServoTowardsL = rec.servoTowardsL;
  setServoAngle((int)lroundf(guideServoPos));
  pausedElapsed = rec.elapsedMs;
  totalEstimatedTime = estimatedTotalMs(targetStepIntervalMicros);
  currentState = 'P';
}

void updateResumeRecord(unsigned long now) {
  static unsigned long lastResumeSave = 0;
  if (resumeAfterPowerLoss && isMotorRunning && spoolingStartTime > 0) {
    if (!resumeSaved || now - lastResumeSave >= RESUME_SAVE_INTERVAL_MS) {
      lastResumeSave = now;
      saveResumeRecord();
    }
  } else if (resumeSaved) {
    clearResumeRecord();
  }
}
