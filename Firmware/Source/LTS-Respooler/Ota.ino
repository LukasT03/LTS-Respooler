// -------------------------- OTA update ---------------------------
// Downloads the firmware from GitHub Pages over Wi-Fi

void sendOTAUpdate() {
  if (otaInProgress) {
    return;
  }
  isMotorRunning = false;
  digitalWrite(STEP_PIN, LOW);
  stepperStop();
  digitalWrite(EN_PIN, HIGH);
  currentState = 'U';
  sendStatus(true);
  xTaskCreate([](void*) {
    const char* ota_url = OTA_URL;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == INADDR_NONE) {
      currentState = 'I';
      sendStatus(true);
      vTaskDelete(NULL);
    }

    otaInProgress = true;
    sendStatus(true);
    if (otaLedPulseTaskHandle == nullptr) {
      xTaskCreate(otaLedPulseTask, "OtaLedPulse", 1024, NULL, 2, &otaLedPulseTaskHandle);
    }

    http.begin(client, ota_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    bool otaSuccess = false;

    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      if (Update.begin(contentLength)) {
        WiFiClient* stream = http.getStreamPtr();
        while (http.connected() && Update.writeStream(*stream) > 0) {
          delay(1);
        }
        if (Update.end() && Update.isFinished()) {
          otaSuccess = true;
          StaticJsonDocument<64> doc;
          doc["OTA_OK"] = true;
          delay(100);
          sendStatus(true);
          String jsonOut;
          serializeJson(doc, jsonOut);
          notifyApps(jsonOut);
          NimBLEDevice::deinit(true);
          delay(1000);
          otaInProgress = false;
          otaLedPulseTaskHandle = nullptr;
          ESP.restart();
        }
      } else {
        StaticJsonDocument<64> doc;
        doc["OTA_OK"] = false;
        String jsonOut;
        serializeJson(doc, jsonOut);
        notifyApps(jsonOut);
        otaInProgress = false;
        otaLedPulseTaskHandle = nullptr;
        currentState = 'I';
        sendStatus(true);
        vTaskDelete(NULL);
      }
    } else {
      StaticJsonDocument<64> doc;
      doc["OTA_OK"] = false;
      String jsonOut;
      serializeJson(doc, jsonOut);
      notifyApps(jsonOut);
      otaInProgress = false;
      otaLedPulseTaskHandle = nullptr;
      currentState = 'I';
      sendStatus(true);
      vTaskDelete(NULL);
    }
    http.end();
    if (!otaSuccess) {
      StaticJsonDocument<64> doc;
      doc["OTA_OK"] = false;
      String jsonOut;
      serializeJson(doc, jsonOut);
      notifyApps(jsonOut);
    }
    otaInProgress = false;
    otaLedPulseTaskHandle = nullptr;
    currentState = 'I';
    sendStatus(true);
    vTaskDelete(NULL);
  },
              "OTAUpdateTask", 8192, NULL, 1, NULL);
}
