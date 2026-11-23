// ----------------------------- Wi-Fi -----------------------------

void wifiScanTask(void* parameter) {
  WiFi.disconnect(true);
  int n = WiFi.scanNetworks();
  StaticJsonDocument<256> ssidDoc;
  JsonArray arr = ssidDoc.createNestedArray("SSID_LIST");
  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    if (rssi > MIN_RSSI_THRESHOLD) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 0 && ssid.length() < 33) {
        arr.add(ssid);
      }
    }
  }
  String jsonOut;
  serializeJson(ssidDoc, jsonOut);
  notifyApps(jsonOut);
  WiFi.scanDelete();
  prefs.begin("respooler", true);
  String ssid = prefs.getString("ssid", "");
  String pwd = prefs.getString("pwd", "");
  prefs.end();
  if (ssid.length() > 0 && pwd.length() > 0) {
    WiFi.begin(ssid.c_str(), pwd.c_str());
  }
  wifiScanTaskHandle = nullptr;
  vTaskDelete(NULL);
}

// Reports the result of WIFI_CONNECT and saves working credentials
void checkWifiConnect() {
  if (wifiConnectInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnectInProgress = false;
      StaticJsonDocument<64> doc;
      doc["WIFI_CONN_RESULT"] = true;
      String jsonOut;
      serializeJson(doc, jsonOut);
      notifyApps(jsonOut);
      prefs.begin("respooler", false);
      prefs.putString("ssid", wifiSSID);
      prefs.putString("pwd", wifiPassword);
      prefs.end();
    } else if (millis() - wifiConnectStartTime > 10000) {
      wifiConnectInProgress = false;
      StaticJsonDocument<64> doc;
      doc["WIFI_CONN_RESULT"] = false;
      String jsonOut;
      serializeJson(doc, jsonOut);
      notifyApps(jsonOut);
    }
  }
}
