#include <BLEDevice.h> 
#include <BLEUtils.h> 
#include <BLEServer.h>

#define LED_CONNECTED 25  // LED 1: ON when devices are connected
#define LED_CONTROL 26    // LED 2: ON when '1' is written

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks { // Function that allows the code to listen for connection events in the background.
    void onConnect(BLEServer* pServer) { //automatically triggers the moment a device connects to the ESP32.
      digitalWrite(LED_CONNECTED, HIGH);
      Serial.println("Device Connected! LED 1 is ON.");
    }

    void onDisconnect(BLEServer* pServer) { triggers when the connected device drops the connection.
      // Force BOTH LEDs off when connection drops
      digitalWrite(LED_CONNECTED, LOW);
      digitalWrite(LED_CONTROL, LOW);
      
      Serial.println("Device Disconnected! Both LEDs are OFF.");
      
      BLEDevice::startAdvertising();
      Serial.println("Waiting for new connection...");
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks { // Create another class to listen for data written to the specific Characteristic UUID.
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();

      if (value.length() > 0) {
        if (value[0] == '1') {
          digitalWrite(LED_CONTROL, HIGH); 
          Serial.println("Received '1' - LED 2 ON");
        } 
        else if (value[0] == '0') {
          digitalWrite(LED_CONTROL, LOW);
          Serial.println("Received '0' - LED 2 OFF");
        }
      }
    }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_CONNECTED, OUTPUT);
  pinMode(LED_CONTROL, OUTPUT);

  digitalWrite(LED_CONNECTED, LOW);
  digitalWrite(LED_CONTROL, LOW);

  BLEDevice::init("ESP32_LED_Control"); // Initializes the BLE hardware and sets the device name
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID); // Creates a BLE Service using the UUID defined at the top of the code.
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();// Configures the Bluetooth advertising
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE is active and broadcasting. Waiting for connection...");
}

void loop() {
  delay(2000); 
}