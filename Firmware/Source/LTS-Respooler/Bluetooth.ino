// --------------------------- Bluetooth ---------------------------

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo&) override {
    deviceConnected = true;
    if (pServer->getConnectedCount() < 2) {
      NimBLEDevice::startAdvertising();
    }
    xTaskCreate([](void*) {
      delay(1100);
      sendStatus(true);
      vTaskDelete(NULL);
    },
                "delayedSend", 2048, NULL, 1, NULL);
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo&, int) override {
    deviceConnected = pServer->getConnectedCount() > 0;
    if (pServer->getConnectedCount() < 2) {
      NimBLEDevice::startAdvertising();
    }
  }
};
class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo&) override {
    std::string rx = pChar->getValue();
    if (!rx.empty()) handleCommand(rx, SRC_BLE);
  }
};

void bleSetup() {
  NimBLEDevice::init(BOARD_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P6);
  NimBLEDevice::setMTU(512);
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setName(BOARD_NAME);
  advData.addServiceUUID(SERVICE_UUID);

  std::string manufacturerData;
  if (boardVariant == 2) {
    manufacturerData = "\xFF\xFF\x02";
  } else {
    manufacturerData = "\xFF\xFF\x01";
  }
  advData.setManufacturerData(manufacturerData);

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
}

void notifyBle(const String& json) {
  if (!deviceConnected || !pCharacteristic) return;
  if (bleNotifyMutex) xSemaphoreTake(bleNotifyMutex, portMAX_DELAY);
  pCharacteristic->setValue(json.c_str());
  pCharacteristic->notify();
  if (bleNotifyMutex) xSemaphoreGive(bleNotifyMutex);
}

void restartAdvertisingIfIdle() {
  if (!deviceConnected && !NimBLEDevice::getAdvertising()->isAdvertising()) {
    NimBLEDevice::startAdvertising();
  }
}
