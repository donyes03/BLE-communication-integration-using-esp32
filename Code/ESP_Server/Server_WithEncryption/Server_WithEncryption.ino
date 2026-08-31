#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "f00528df-a08e-4c05-a6f4-c3c71dced5a6"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

//Security Callbacks to handle the PIN code
class MySecurityCallbacks : public BLESecurityCallbacks {
  void onPassKeyNotify(uint32_t pass_key) {
    Serial.print(">>> PAIRING PIN: ");
    Serial.printf("%06d\n", pass_key); // Prints the 6-digit PIN to the Serial Monitor
  }
  bool onSecurityRequest() { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
    if (cmpl.success) Serial.println("Pairing successful! Encrypted.");
    else Serial.println("Pairing failed!");
  }
  uint32_t onPassKeyRequest() { return 0; }
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
  
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pServer->advertiseOnDisconnect(true);
  
  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    
  // Lock the characteristic to require encryption ---
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    
  pCharacteristic->setValue("HELLO! Donyes");
  pService->start();

  //Turn on the BLE Security Manager
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT); // Tells the PC we have a display (Serial Monitor) to show a PIN
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // 0x06 * 1.25ms = 7.5ms
  pAdvertising->setMaxPreferred(0x12); // 0x12 * 1.25ms = 22.5ms
  BLEDevice::startAdvertising();
  Serial.println("Encrypted Characteristic defined! Waiting for secure connection...");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
}