#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "180F"
#define CHARACTERISTIC_UUID "180D"

BLEServer* pServer = NULL;
bool startAdvertisingAgain = false;
int connectedDevices = 0;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    connectedDevices++;
    Serial.printf("Device connected! Total connections: %d\n", connectedDevices);
    
    startAdvertisingAgain = true; 
  }

  void onDisconnect(BLEServer* pServer) {
    connectedDevices--;
    Serial.printf("Device disconnected! Total connections: %d\n", connectedDevices);
    
    startAdvertisingAgain = true;
  }
};

void setup() {
  Serial.begin(115200);

  if (!BLEDevice::init("ESP32")) {
    Serial.println("BLE initialization failed!");
    return;
  }
  else {
    Serial.println("BLE initialization succeeded!");
  }
  
  pServer = BLEDevice::createServer();
  
  // Attach the server callbacks to track connections!
  pServer->setCallbacks(new MyServerCallbacks()); 
  
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
      
  pCharacteristic->setValue("Hello! Donyes");
  pService->start();


  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMaxPreferred(0x12); 
  
  BLEDevice::startAdvertising();
  Serial.println("BLE Server is running! Waiting for connections...");
}

void loop() {
  // We do this here rather than the callback to prevent crashing the BLE stack
  if (startAdvertisingAgain) {
    delay(500); 
    pServer->startAdvertising(); 
    Serial.println("Advertising restarted. Ready for the next device to connect.");
    startAdvertisingAgain = false;
  }
  
  delay(100);
}