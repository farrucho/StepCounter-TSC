#include <ArduinoBLE.h>

// Create a service UUID (random)
BLEService stepService("8e078479-a26f-4471-a7c3-81209ffff3c6");

// Create a characteristic for data streaming
BLECharacteristic dataChar(
  "506cad0b-684a-4666-91c7-56d4490b4acc",
  BLERead | BLENotify,
  100 // max length of your data string
);

void setup() {
  if (!BLE.begin()) {
    while (1);
  }

  BLE.setLocalName("AI StepSensor");
  BLE.setAdvertisedService(stepService);

  stepService.addCharacteristic(dataChar);
  BLE.addService(stepService);

  dataChar.writeValue("Starting...");

  BLE.advertise();
}


void loop(){
    // Build data string
        char buffer[100];
      sprintf(buffer, "%.4f,%.4f,%.4f,%s,%.2f,%d",
              1.0, 1.0, 1.0,
              "TESTESTATE",
              1.0,
              0);

        // Send via Serial (optional)
        Serial.println(buffer);

        // Send via BLE
        dataChar.writeValue(buffer);

        // Let BLE handle connections
        BLE.poll();
}