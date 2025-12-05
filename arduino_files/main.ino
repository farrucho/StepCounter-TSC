// Wire1 needs to be used in this board to communicate between accelerometer and gyro
#include <Wire.h>
// State Machine Algorithm Implementation Library
#include "StepDetector.h"
// Bluetooth
#include <ArduinoBLE.h>

// Create a random service UUID
BLEService stepService("8e078479-a26f-4471-a7c3-81209ffff3c6");

// Create a characteristic for data streaming
BLECharacteristic dataChar(
  "506cad0b-684a-4666-91c7-56d4490b4acc",
  BLERead | BLENotify,
  128 // maximum number of bytes in data string
);

// create state machine object
StepDetector stepDetector;

/* Helper Functions */
// documentation used: DocID025715 Rev 3
#define LSM9DS1_ADDRESS            0x6b

#define LSM9DS1_WHO_AM_I           0x0f // read only
#define LSM9DS1_CTRL_REG1_G        0x10
#define LSM9DS1_STATUS_REG         0x17
#define LSM9DS1_OUT_X_G            0x18
#define LSM9DS1_CTRL_REG6_XL       0x20
#define LSM9DS1_CTRL_REG8          0x22
#define LSM9DS1_OUT_X_XL           0x28


#define LSM9DS1_CTRL_REG1_M        0x20
#define LSM9DS1_CTRL_REG2_M        0x21
#define LSM9DS1_CTRL_REG3_M        0x22
#define LSM9DS1_STATUS_REG_M       0x27
#define LSM9DS1_OUT_X_L_M          0x28



void writeRegister(uint8_t slaveAddress, uint8_t reg, uint8_t val) {
    Wire1.beginTransmission(slaveAddress);
    Wire1.write(reg);
    Wire1.write(val);
    Wire1.endTransmission();
}


int readRegister(uint8_t slaveAddress, uint8_t address)
{
  // this function helps to read the addresss inside of the sensor
  // slave is like the street name and address is the mailbox


  Wire1.beginTransmission(slaveAddress);
  Wire1.write(address);

  // check if the slave receive the request properly
  if (Wire1.endTransmission() != 0) {
    return -1;
  }

  // check if information came back
  if (Wire1.requestFrom(slaveAddress, 1) != 1) {
    return -1;
  }

  return Wire1.read();
}


void end()
{
  writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG1_G, 0x00);
  writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG6_XL, 0x00);

  Wire1.end();
}


int begin()
{
  // 5.1.1 do manual
  Wire1.begin();

  // reset
  writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG8, 0b00000101);

  delay(10);


  // if accelerometer and gyroscope read only register WHO_AM_I can be correctly read then the I2C is working properly 
  // 7.11
  // table 43
  if (readRegister(LSM9DS1_ADDRESS, LSM9DS1_WHO_AM_I) != 0b01101000) {
    // Serial.print(readRegister(LSM9DS1_ADDRESS, LSM9DS1_WHO_AM_I)); // print to see what is happening
    end();
    return 0;
  }

  // Configure Accelerometer
  // Table 66. CTRL_REG6_XL register
  // ODR_XL2 ODR_XL1 ODR_XL0 FS1_XL FS0_XL BW_SCAL _ODR BW_XL1 BW_XL0
  // ODR_XL (7:5) : 100 -> 238 Hz
  // FS_XL (4:3) : 11 -> +-8g (boa resolucao (usain bolt chega a aprox 1g de aceleracao para contar passos isto basta e assim o sinal NAO SATURA))
  // BW_SCAL_ODR (2:2): 1 -> if 0 bandwidth determined by ODR selection, otherwise manual
  // BW_XL (2:0): 10 -> 105 Hz
  writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG6_XL, 0b10011110);

  // Configure Gyroscope, we did not use it in the end, but maybe in a nearby future...
  // Table 44. CTRL_REG1_G register
  // ODR_G2 ODR_G1 ODR_G0 FS_G1 FS_G0 0 (1) BW_G1 BW_G0
  // ODR_G (7:5) : 100 -> 238 Hz
  // FS_G (4:3) : 01 -> ±500 dps (degrees per second)
  // 0 (2:2) : additional bit that needs to be 0
  // BW_G (1:0) : 01 -> bandwidth cutoff 29 Hz (reduce noise)
  // writeRegister(LSM9DS1_ADDRESS, LSM9DS1_CTRL_REG1_G, 0b10001001);

  return 1;
}


/* Ultrasound Sensor */
const int trigPin = 6;
const int echoPin = 10;

volatile unsigned long tBegin = 0;
volatile unsigned long tEnd = 0;
volatile bool echoDone = false;
volatile bool awaitingEcho = false;

float currentDistance = 0.0f; // stores the latest distance in cm
unsigned long lastPingMs = 0;

float maxPeakDistance = 0.0f; // stores the first Max distance in cm

String LastStateStr = "";



void echoISR() {
  if (!awaitingEcho) return;
  if (digitalRead(echoPin) == HIGH) {
    tBegin = micros();
  } else {
    tEnd = micros();
    echoDone = true;
    awaitingEcho = false;
  }
}

void triggerPing() {
  // only trigger if we aren't already waiting for a response
  // if (!awaitingEcho) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    awaitingEcho = true;
  // }
}



/* Main Functions */

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  if (!BLE.begin()) {
    // Serial.begin(115200); // used when recording training data
    while (1);
  }

  // while (!Serial);
  
  if (!begin()) {
  //     Serial.println("Failed to initialize IMU!");
      while (1);
  }

  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);

  // Configure the Step Detector Parameters
  stepDetector.setAlpha(0.0679); 
  stepDetector.setWindowSize(10);

  // Serial.println("Initialize correctly IMU!");  
  // Serial.println("Step Detector Configured. Starting...");
  // Serial.println("ax,ay,az,state,step_detected");
  
  
  // Serial.print("Accelerometer sample rate = ");
  // Serial.print(IMU.accelerationSampleRate());
  // Serial.println(" Hz");


  BLE.setLocalName("MLP-StepSensor");
  BLE.setAdvertisedService(stepService);

  stepService.addCharacteristic(dataChar);
  BLE.addService(stepService);

  dataChar.writeValue("Starting...");

  BLE.advertise();
}


unsigned long lastFreqPrint = 0;
volatile unsigned long sampleCount = 0;

void loop() {
  if (echoDone) {
    noInterrupts();
    unsigned long us = tEnd - tBegin;
    echoDone = false;
    interrupts();

    // ler apenas valores reais e descartar timeouts
    if (us <= 69000) { 
      currentDistance = (us * 0.0343f) / 2.0f;
    }

  }



  String currentStateStr = stepDetector.getCurrentState();

  // Serial.print(currentStateStr);

  if (currentStateStr == "LOOKING_FOR_MIN" and (LastStateStr == "LOOKING_FOR_FIRST_MAX" || LastStateStr == "LOOKING_FOR_SECOND_MAX")) {
    maxPeakDistance = currentDistance;
  }

  LastStateStr = currentStateStr;

  // Serial.print(maxPeakDistance);



  // quando não estamos à espera de ECHO e já passou o intervalo, volta a disparar
  if (!awaitingEcho && !echoDone) {
    triggerPing();
  }

  // try to read status register
  int status = readRegister(LSM9DS1_ADDRESS, LSM9DS1_STATUS_REG);
  if (status < 0) {
    // Serial.println("Error reading status register");
    return;
  }

  static unsigned long lastSampleTime = 0;
  unsigned long now = millis();
  
  // Force 200Hz (1000 / 200 = 5ms)
  if (now - lastSampleTime < 5) {
      return;
  }
  lastSampleTime = now;

  // bit XLDA (0: a new set of data is not yet available; 1: a new set of data is available)
  if (status & 0x01) {
    // 5.1.1 I2C operation
    // 3.3 Accelerometer and gyroscope multiple reads (burst)

    // get the "street"
    Wire1.beginTransmission(LSM9DS1_ADDRESS);
    // get the "mailbox", IF_ADD_INC Bit set to 1, 
    // auto increment and will start reading from register 0x28
    Wire1.write(LSM9DS1_OUT_X_XL | 0b10000000); 

    if (Wire1.endTransmission() != 0) {
      // Serial.println("Error addressing acc data");
      return;
    }
    
    // requests 6 bytes starting at address OUT_X_XL with auto-increment
    if (Wire1.requestFrom(LSM9DS1_ADDRESS, (uint8_t)6) != 6) { // number of bytes received must be 6
      // Serial.println("Error requesting acc data");
      return;
    }


    // Accelerometer and gyroscope register description
    // 7.31 OUT_X_XL (28h - 29h) 
    // 7.32 OUT_Y_XL (2Ah - 2Bh)
    // 7.33 OUT_Z_XL (2Ch - 2Dh)
    uint8_t xl = Wire1.read(); // 0x28h x_low byte
    uint8_t xh = Wire1.read(); // 0x29h x_high byte
    uint8_t yl = Wire1.read(); // 0x2Ah
    uint8_t yh = Wire1.read(); // 0x2Bh
    uint8_t zl = Wire1.read(); // 0x2Ch
    uint8_t zh = Wire1.read(); // 0x2Dh

    int16_t x_raw = (int16_t)(xh << 8 | xl); // x_raw = first byte from register 0x28h and second byte from register 0x29h
    int16_t y_raw = (int16_t)(yh << 8 | yl); // and so on
    int16_t z_raw = (int16_t)(zh << 8 | zl); // ...

    // print real values
    // 2.1 Sensor characteristics
    // Linear acceleration sensitivity for 8g is 0.244 mg/LSB
    float ax = x_raw*0.244/1000; // convert raw to g
    float ay = y_raw*0.244/1000;
    float az = z_raw*0.244/1000;






    float ultrasound = 0.0f;

    bool stepWasDetected = stepDetector.process(ax, ay, az);
    
    if (stepWasDetected){
      ultrasound = abs(maxPeakDistance - currentDistance);
    }

    // used when recording training data
    // Serial.print(ax, 6);
    // Serial.print(",");
    // Serial.print(ay, 6);
    // Serial.print(",");
    // Serial.print(az, 6);
    // Serial.print(",");
    // Serial.print(stepDetector.getCurrentState());
    // Serial.print(",");
    // Serial.print(ultrasound);
    // Serial.print(",");
    // Serial.println(stepWasDetected ? "1" : "0");


    
    char buffer[100];
    sprintf(buffer, "%.6f,%.6f,%.6f,%s,%.6f,%d",
      ax, ay, az,
      stepDetector.getCurrentState(),
      ultrasound,
      stepWasDetected ? 1 : 0,
      maxPeakDistance,
      currentDistance
    );
    

    // Serial.print(maxPeakDistance);
    // Serial.print("-");
    // Serial.print(currentDistance);
    // Serial.print("\n");
    dataChar.writeValue(buffer);


    
    // count this sample
    sampleCount++;
  }
  BLE.poll();
  // Print serial sending data frequency every 3000 ms
  // unsigned long now = millis();
  // if (now - lastFreqPrint >= 3000) {
  //   Serial.print("Measured Frequency: ");
  //   Serial.print(sampleCount);
  //   Serial.println(" Hz"); 

  //   sampleCount = 0;
  //   lastFreqPrint = now;
  // }
}
