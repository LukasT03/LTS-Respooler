// ---------------------------- Status -----------------------------

#if CONTROL_BOARD
int readVbusMillivolts() {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogReadMilliVolts(VBUS_SENSE_PIN);
  return (int)lroundf((sum / 4.0f) * VBUS_DIVIDER);
}
#endif

void sendStatus(bool forceSend) {
  if (!(deviceConnected && pCharacteristic) && !networkWantsStatus(forceSend)) return;

  float progVal = progress;
  int remVal = 0;
  if (currentState == 'D') {
    progVal = 100.0f;
    remVal = 0;
  } else if (totalEstimatedTime > 0) {
    unsigned long elapsed;
    if (spoolingStartTime > 0) {
      elapsed = millis() - spoolingStartTime;
    } else if (pausedElapsed > 0) {
      elapsed = pausedElapsed;
    } else if (currentState == 'A') {
      elapsed = pausedElapsed;
    } else {
      elapsed = 0;
    }
    unsigned long effectiveTotal = totalEstimatedTime;
    if (targetWeight == 2) effectiveTotal = (unsigned long)(totalEstimatedTime * TARGET_WEIGHT_FACTOR_2);
    else if (targetWeight == 3) effectiveTotal = (unsigned long)(totalEstimatedTime * TARGET_WEIGHT_FACTOR_3);
    if (effectiveTotal == 0) {
      progVal = 0.0f;
      remVal = 0;
    } else if (elapsed >= effectiveTotal) {
      progVal = 100.0f;
      remVal = 0;
    } else {
      progVal = (100.0f * (float)elapsed) / (float)effectiveTotal;
      unsigned long remainingMs = effectiveTotal - elapsed;
      remVal = (int)(remainingMs / 1000UL);
    }
  }
  if (remHoldActive) {
    unsigned long nowMs = millis();
    if (nowMs < remHoldExpiry) {
      remVal = lastRemainingTime;
    } else {
      remHoldActive = false;
    }
  }
  if (progHoldActive) {
    unsigned long nowMs = millis();
    if (nowMs < progHoldExpiry) {
      progVal = lastProgress;
    } else {
      progHoldActive = false;
    }
  }
  if (currentState == 'R') {
    unsigned long nowMs = millis();
    if (nowMs < remStartHoldUntil) {
      remVal = lastRemainingTime;
    }
  }
  int chipTemp = (int)temperatureRead();

  StaticJsonDocument<640> doc;
  doc["STAT"] = String(currentState);
  doc["HAS_FIL"] = filamentDetected;
  doc["USE_FIL"] = useFilamentSensor;
  doc["FIL_T"] = filamentLossMs;
  doc["RES"] = resumeAfterPowerLoss;
  doc["PROG"] = progVal;
  doc["REM"] = remVal;
  doc["TEMP"] = chipTemp;
#if CONTROL_BOARD
  doc["VBUS_MV"] = readVbusMillivolts();
  doc["PG"] = digitalRead(PD_GOOD_PIN) == LOW;
#endif
  doc["WIFI_SSID"] = wifiSSID.length() > 0 ? wifiSSID.c_str() : nullptr;
  doc["WIFI_OK"] = WiFi.status() == WL_CONNECTED;
  doc["FW"] = FIRMWARE_VERSION;
  doc["ID"] = boardId();
  doc["NAME"] = boardName.c_str();
  doc["REMOTE"] = remoteAccess;
  doc["CLOUD"] = networkCloudConnected();
  if (WiFi.status() == WL_CONNECTED) doc["IP"] = WiFi.localIP().toString();
  else doc["IP"] = nullptr;
  doc["OTA_OK"] = nullptr;
  doc["SPD"] = speedPercent;
  doc["JIN"] = jingleStyle;
  doc["LED"] = ledBrightness;
  doc["DIR"] = motorDirection;
  doc["POW"] = motorStrength;
  doc["TRQ"] = highSpeedMode ? 0 : torqueLimit;
  doc["WGT"] = targetWeight;
  doc["DUR"] = calibrationAt85Speed / 1000;
  doc["HS"] = highSpeedMode;
  doc["FAN_SPD"] = fanSpeed;
  doc["FAN_ON"] = (fanAlwaysOn || isMotorRunning || millis() < fanStopAfter);
  doc["FAN_ALW"] = fanAlwaysOn;
  doc["SV_R"] = servoAngleR;
  doc["SV_L"] = servoAngleL;
  doc["SV_STP"] = servoStepMm;
  doc["SV_HOME"] = servoHomeIsR ? "R" : "L";
  doc["VAR"] = (boardVariant == 2) ? "PRO" : (boardVariant == 1) ? "STD"
                                                                 : "UNK";

  lastIsMotorRunning = isMotorRunning;
  lastFilamentDetected = filamentDetected;
  lastProgress = progVal;
  lastChipTemperature = chipTemp;
  lastRemainingTime = remVal;
  lastSpeedPercent = speedPercent;

  String jsonOut;
  serializeJson(doc, jsonOut);
  notifyBle(jsonOut);
  networkSendStatus(jsonOut, forceSend);
}

// Replies and events for the app, over BLE and the local network
void notifyApps(const String& json) {
  notifyBle(json);
  localSend(json);
}
