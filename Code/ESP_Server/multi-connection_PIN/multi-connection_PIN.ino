#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

/*#define SERVICE_UUID        "f00528df-a08e-4c05-a6f4-c3c71dced5a6"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"*/

#define SERVICE_UUID        "180F"
#define CHARACTERISTIC_UUID "180D"

BLEServer* pServer = NULL;
bool startAdvertisingAgain = false;
int connectedDevices = 0;

// to handle what happens when a device connects or disconnects
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


class MySecurityCallbacks : public BLESecurityCallbacks {
  void onPassKeyNotify(uint32_t pass_key) {
    Serial.print(">>> PAIRING PIN: ");
    Serial.printf("%06d\n", pass_key); // Prints the 6-digit PIN to the Serial Monitor
  }
  
  bool onSecurityRequest() { return true; } // Accept pairing requests

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) { 
    if (cmpl.success) Serial.println("Pairing successful! Encrypted.");
    else Serial.println("Pairing failed!");
  }
  
  uint32_t onPassKeyRequest() { return 0; } //so the BLE system knows it should never ask the ESP32 to input a PIN
  bool onConfirmPIN(uint32_t pass_key) { return true; }
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
    
  // Lock the characteristic to require encryption
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    
  pCharacteristic->setValue("HELLO! Donyes");
  pService->start();

  // Turn on the BLE Security Manager
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT); // to tell esp to display PIN
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK); //encryption and authentication keys

  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMaxPreferred(0x12); 
  
  BLEDevice::startAdvertising();
  Serial.println("Encrypted Characteristic defined! Waiting for secure connections...");
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