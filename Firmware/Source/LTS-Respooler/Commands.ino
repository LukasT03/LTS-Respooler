// --------------------------- Commands ---------------------------
// JSON commands (CMD) and settings (SET) from BLE, the local network, LTS Cloud and USB serial

void pollSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      serialRxLine.trim();
      if (serialRxLine.length() > 0) {
        if (serialRxLine.length() > 512) {
          serialRxLine = "";
        } else {
          std::string cmd(serialRxLine.c_str());
          handleCommand(cmd, SRC_LOCAL);
          serialRxLine = "";
        }
      } else {
        serialRxLine = "";
      }
    } else {
      if (serialRxLine.length() < 512) serialRxLine += c;
    }
  }
}

void handleCommand(const std::string& cmd, CommandSource src) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, cmd.c_str());
  if (error) return;

  if (doc.containsKey("CMD")) {
    String command = doc["CMD"].as<String>();
    if (src == SRC_CLOUD && (command == "WIFI_SCAN" || command == "WIFI_CONNECT" || command == "OTA")) return;
    if (command == "START") {
      if (!isMotorRunning) {
        shouldStartMotorNow = true;
        delayStartUntil = millis();
      }
    } else if (command == "STOP") {
      if (otaInProgress) {
        return;
      }
      if (currentState == 'A') {
        remHoldActive = true;
        remHoldExpiry = millis() + 200;
        progHoldActive = true;
        progHoldExpiry = millis() + 200;
        isMotorRunning = false;
        shouldStartMotorNow = false;
        pendingDirectionChange = false;
        triggerJingleNow = 0;
        progress = 0.0;
        totalEstimatedTime = 0;
        spoolingStartTime = 0;
        pausedElapsed = 0;
        filamentLostSince = 0;
        stepperStop();
        ledc_stop(LEDC_LOW_SPEED_MODE, STEP_LEDC_CHANNEL, 0);
        gpio_reset_pin((gpio_num_t)STEP_PIN);
        gpio_set_direction((gpio_num_t)STEP_PIN, GPIO_MODE_OUTPUT);
        digitalWrite(STEP_PIN, LOW);
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
        currentState = 'I';
        sendStatus(true);
      } else {
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
      }
    } else if (command == "PAUSE") {
      if (isMotorRunning) {
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
    } else if (command == "KEY") {
      if (src != SRC_CLOUD) sendBoardKey();
    } else if (command == "WIFI_SCAN") {
      if (wifiScanTaskHandle == nullptr) {
        xTaskCreate(wifiScanTask, "WiFiScan", 4096, NULL, 1, &wifiScanTaskHandle);
      }
    } else if (command == "OTA") {
      if (!otaInProgress) {
        sendOTAUpdate();
      }
    } else if (command == "WIFI_CONNECT") {
      if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
        WiFi.disconnect(true);
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        wifiConnectInProgress = true;
        wifiConnectStartTime = millis();
      }
    }
  }
  if (doc.containsKey("SET")) {
    JsonObject set = doc["SET"].as<JsonObject>();
    if (src == SRC_CLOUD) {
      set.remove("WIFI_SSID");
      set.remove("WIFI_PASS");
      set.remove("REMOTE");
    }
    bool changed = false;
    bool servoEndpointsChanged = false;
    if (set.containsKey("VAR")) {
      JsonVariant vj = set["VAR"];
      uint8_t newVar = boardVariant;
      if (vj.is<const char*>()) {
        const char* s = vj.as<const char*>();
        if (s && s[0]) {
          String up = String(s);
          up.toUpperCase();
          if (up == "PRO") newVar = 2;
          else if (up == "STD" || up == "STANDARD") newVar = 1;
          else if (up == "UNK" || up == "UNKNOWN") newVar = 0;
        }
      } else {
        int vi = vj.as<int>();
        if (vi < 0) vi = 0;
        if (vi > 2) vi = 2;
        newVar = (uint8_t)vi;
      }
      if (newVar != boardVariant) {
        boardVariant = newVar;
        changed = true;
      }
    }
    if (set.containsKey("DIR")) {
      int dir = set["DIR"];
      if (dir != motorDirection) {
        if (isMotorRunning) {
          pendingDirectionChange = true;
          newMotorDirection = dir;
          isMotorRunning = false;
          stepperStop();
          digitalWrite(EN_PIN, HIGH);
          digitalWrite(STEP_PIN, LOW);
        } else {
          motorDirection = dir;
          digitalWrite(DIR_PIN, motorDirection);
          prefs.begin("respooler", false);
          prefs.putUInt("dir", motorDirection);
          prefs.end();
        }
        changed = true;
      }
    }
    if (set.containsKey("LED")) {
      ledBrightness = constrain((int)set["LED"], 0, 100);
      pwmValue = calculatePWM(ledBrightness);
      prefs.begin("respooler", false);
      prefs.putUInt("led", ledBrightness);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("USE_FIL")) {
      useFilamentSensor = (int)set["USE_FIL"] != 0;
      prefs.begin("respooler", false);
      prefs.putBool("filamentSensor", useFilamentSensor);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("RES")) {
      resumeAfterPowerLoss = (int)set["RES"] != 0;
      prefs.begin("respooler", false);
      prefs.putBool("resume", resumeAfterPowerLoss);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("FIL_T")) {
      filamentLossMs = constrain((int)set["FIL_T"], 100, 400);
      prefs.begin("respooler", false);
      prefs.putUInt("filLossMs", filamentLossMs);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("POW")) {
      motorStrength = constrain((int)set["POW"], 80, 120);
      setMotorCurrent(motorStrength);
      prefs.begin("respooler", false);
      prefs.putUInt("motorStrength", motorStrength);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("TRQ")) {
      int newTrq = constrain((int)set["TRQ"], 0, 3);
      if (highSpeedMode) {
        savedTorqueLimitHS = newTrq;
        prefs.begin("respooler", false);
        prefs.putUInt("torqueLimit", savedTorqueLimitHS);
        prefs.end();
        torqueLimit = 0;
      } else {
        torqueLimit = newTrq;
        prefs.begin("respooler", false);
        prefs.putUInt("torqueLimit", torqueLimit);
        prefs.end();
      }
      changed = true;
    }
    if (set.containsKey("JIN")) {
      int newJingleStyle = constrain((int)set["JIN"], 0, 3);
      if (newJingleStyle != jingleStyle) {
        jingleStyle = newJingleStyle;
        prefs.begin("respooler", false);
        prefs.putUInt("jingle", jingleStyle);
        prefs.end();
        if (!isMotorRunning) triggerJingleNow = jingleStyle;
        changed = true;
      }
    }
    if (set.containsKey("DUR")) {
      unsigned long dur = (unsigned long)set["DUR"];
      if (dur >= 10 && dur <= 20000) {
        calibrationAt85Speed = dur * 1000UL;
        prefs.begin("respooler", false);
        prefs.putULong("cal85", calibrationAt85Speed);
        prefs.end();
        changed = true;
      }
    }
    if (set.containsKey("WGT")) {
      int newWgt = constrain((int)set["WGT"], 0, 3);
      targetWeight = newWgt;
      prefs.begin("respooler", false);
      prefs.putUInt("targetWeight", (unsigned int)targetWeight);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("SPD")) {
      int speed = constrain((int)set["SPD"], 50, 100);
      double span = 170.0 - 93.0;
      double baseInterval = 170.0 - ((double)(speed - 50) * span / 50.0);
      baseTargetStepIntervalMicros = (unsigned long)lround(baseInterval);
      speedPercent = speed;
      prefs.begin("respooler", false);
      prefs.putUInt("speed", baseTargetStepIntervalMicros);
      prefs.end();
      applySpeedTarget();
      changed = true;
    }
    if (set.containsKey("HS")) {
      bool newHS = (int)set["HS"] != 0;
      if (newHS != highSpeedMode) {
        if (newHS) {
          highSpeedMode = true;
          savedTorqueLimitHS = torqueLimit;
          torqueLimit = 0;
          applySpeedTarget();
          prefs.begin("respooler", false);
          prefs.putBool("hsMode", true);
          prefs.end();
        } else {
          highSpeedMode = false;
          torqueLimit = savedTorqueLimitHS;
          applySpeedTarget();
          prefs.begin("respooler", false);
          prefs.putBool("hsMode", false);
          prefs.putUInt("torqueLimit", torqueLimit);
          prefs.end();
        }
#if HIGH_SPEED_SPREADCYCLE
        driver.en_spreadCycle(highSpeedMode);
#endif
        changed = true;
      }
    }
    if (set.containsKey("FAN_SPD")) {
      fanSpeed = constrain((int)set["FAN_SPD"], 0, 100);
      prefs.begin("respooler", false);
      prefs.putUInt("fanSpeed", fanSpeed);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("FAN_ALW")) {
      fanAlwaysOn = (int)set["FAN_ALW"] != 0;
      prefs.begin("respooler", false);
      prefs.putBool("fanAlways", fanAlwaysOn);
      prefs.end();
      changed = true;
    }

    if (set.containsKey("SV_R")) {
      int r = constrain((int)set["SV_R"], 0, 180);
      if (r != servoAngleR) {
        servoAngleR = r;
        prefs.begin("respooler", false);
        prefs.putUInt("servoR", (unsigned int)servoAngleR);
        prefs.end();
        servoEndpointsChanged = true;
        changed = true;
      }
      if (!isMotorRunning && (currentState == 'I' || currentState == 'D') && servoGotoSide == 'R') {
        servoGotoActive = true;
        servoHomingToHome = false;
        lastServoHomeStepTime = millis();
      }
    }
    if (set.containsKey("SV_L")) {
      int l = constrain((int)set["SV_L"], 0, 180);
      if (l != servoAngleL) {
        servoAngleL = l;
        prefs.begin("respooler", false);
        prefs.putUInt("servoL", (unsigned int)servoAngleL);
        prefs.end();
        servoEndpointsChanged = true;
        changed = true;
      }
      if (!isMotorRunning && (currentState == 'I' || currentState == 'D') && servoGotoSide == 'L') {
        servoGotoActive = true;
        servoHomingToHome = false;
        lastServoHomeStepTime = millis();
      }
    }
    if (set.containsKey("SV_STP")) {
      float s = (float)set["SV_STP"].as<float>();
      s = constrain(s, 0.05f, 20.0f);
      if (fabsf(s - servoStepMm) > 0.0001f) {
        servoStepMm = s;
        prefs.begin("respooler", false);
        prefs.putFloat("servoStep", servoStepMm);
        prefs.end();
        changed = true;
      }
    }
    if (set.containsKey("SV_HOME")) {
      const char* v = set["SV_HOME"].as<const char*>();
      char c = (v && v[0]) ? v[0] : 0;
      if (c >= 'a' && c <= 'z') c = (char)(c - 32);
      if (c == 'R' || c == 'L') {
        bool homeR = (c == 'R');
        if (homeR != servoHomeIsR) {
          servoHomeIsR = homeR;
          prefs.begin("respooler", false);
          prefs.putBool("servoHomeR", servoHomeIsR);
          prefs.end();
          servoEndpointsChanged = true;
          changed = true;
        }
      }
    }
    if (set.containsKey("SV_GOTO") && !isMotorRunning && (currentState == 'I' || currentState == 'D')) {
      const char* v = set["SV_GOTO"].as<const char*>();
      char c = (v && v[0]) ? v[0] : 0;
      if (c >= 'a' && c <= 'z') c = (char)(c - 32);

      if (c == 'L' || c == 'R' || c == 'H') {
        servoGotoSide = c;
        servoGotoActive = true;
        servoHomingToHome = false;
        lastServoHomeStepTime = millis();
      }
    }
    if (servoEndpointsChanged && !isMotorRunning && (currentState == 'I' || currentState == 'D') && !servoGotoActive && !(servoGotoSide == 'L' || servoGotoSide == 'R')) {
      guideServoTowardsL = servoHomeIsR;
      servoHomingToHome = true;
      lastServoHomeStepTime = millis();
    }
    if (set.containsKey("NAME")) {
      String name = set["NAME"].as<String>();
      name.trim();
      if (name.length() <= 64 && name != boardName) {
        boardName = name;
        prefs.begin("respooler", false);
        prefs.putString("name", boardName);
        prefs.end();
        changed = true;
      }
    }
    if (set.containsKey("REMOTE")) {
      remoteAccess = set["REMOTE"].as<bool>();
      prefs.begin("respooler", false);
      prefs.putBool("remote", remoteAccess);
      prefs.end();
      changed = true;
    }
    if (set.containsKey("WIFI_SSID")) {
      wifiSSID = set["WIFI_SSID"].as<String>();
      changed = true;
    }
    if (set.containsKey("WIFI_PASS")) {
      wifiPassword = set["WIFI_PASS"].as<String>();
      changed = true;
    }
    if (changed) {
      prefs.begin("respooler", false);
      prefs.putString("ssid", wifiSSID);
      prefs.putString("pwd", wifiPassword);
      prefs.putUChar("variant", boardVariant);
      prefs.end();
      sendStatus(true);
    }
  }
}
