#include <Wire.h>
#include "LSM9DS1.h"

// unsigned long lastMillis = 0;
// unsigned long count = 0;

#define LSM9DS1_ADDR  0x6B


#define CTRL_REG1_G   0x10
#define CTRL_REG6_XL  0x20


// esta privado na library, clonado para estar publico e usável
void writeRegister(uint8_t slaveAddress, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(slaveAddress);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    if (!IMU_LSM9DS1.begin()) {
        Serial.println("Failed to initialize LSM9DS1!");
        while (1);
    }

    Wire.begin();

    
    // Configure Accelerometer
    // Table 66. CTRL_REG6_XL register
    // ODR_XL2 ODR_XL1 ODR_XL0 FS1_XL FS0_XL BW_SCAL _ODR BW_XL1 BW_XL0
    // ODR_XL (7:5) : 110 -> 952 Hz
    // FS_XL (4:3) : 11 -> +-8g
    // BW_SCAL_ODR (2:2): 0 -> bandwidth determined by ODR selection, in this case bandwidth will be 408 Hz
    // BW_XL (2:0): 11 -> 408 Hz
    writeRegister(LSM9DS1_ADDR, CTRL_REG6_XL, 0b11011011);

    // Configure Gyroscope
    // Table 44. CTRL_REG1_G register
    // ODR_G2 ODR_G1 ODR_G0 FS_G1 FS_G0 0 (1) BW_G1 BW_G0
    // ODR_G (7:5) : 110 -> 952 Hz
    // FS_G (4:3) : 10 -> ±2000 dps
    // 0 (2:2) : additional bit that needs to be 0
    // BW_G (1:0) : 11 -> bandwidth cutoff 100 Hz
    writeRegister(LSM9DS1_ADDR, CTRL_REG1_G, 0b11010011);


    Serial.println("LSM9DS1 accel and gyro set to 952 Hz");
}



// otmizado para apenas print quando há valores novos
void loop() {
  float ax, ay, az, gx, gy, gz;

  if (IMU_LSM9DS1.accelerationAvailable() && IMU_LSM9DS1.gyroscopeAvailable()) {
    if (IMU_LSM9DS1.readAcceleration(ax, ay, az) && IMU_LSM9DS1.readGyroscope(gx, gy, gz)) {
      Serial.print(ax, 3);
      Serial.print(",");
      Serial.print(ay, 3);
      Serial.print(",");
      Serial.print(az, 3);
      Serial.print(",");
      Serial.print(gx, 3);
      Serial.print(",");
      Serial.print(gy, 3);
      Serial.print(",");
      Serial.println(gz, 3);

      // count++;
      // unsigned long now = millis();
      // if (now - lastMillis >= 1000) {
      //   Serial.print("Samples per second: ");
      //   Serial.println(count);
      //   count = 0;
      //   lastMillis = now;
      // }
    }
  }
}