#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>


#define SERVICE_UUID        "f00528df-a08e-4c05-a6f4-c3c71dced5a6"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

void setup() {
  Serial.begin(115200);

  if (!BLEDevice::init("ESP32")) {
    Serial.println("BLE initialization failed!");
    return;
  }
  else {
    Serial.println("BLE initialization succeeded!");
  }
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pServer->advertiseOnDisconnect(true);
  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setValue("HELLO! Donyes");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // 0x06 * 1.25ms = 7.5ms (The fastest allowed connection interval)
  pAdvertising->setMaxPreferred(0x12); // 0x12 * 1.25ms = 22.5ms
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}

