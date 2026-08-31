#include <BLEDevice.h>
#include <BLEServer.h>

void setup() {
  Serial.begin(9600);
  delay (100);
  BLEDevice::init("Test");
  Serial.println("BLE initialized");
}
void loop() {}
