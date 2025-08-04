#include "DistanceSensor.h"

DistanceSensor::DistanceSensor(uint8_t loxSHT)
    : loxAddress(0x29), loxSHT(loxSHT), lox() {
    pinMode(loxSHT, OUTPUT);
    digitalWrite(loxSHT, LOW);  // Keep sensor off until initialized
}
void DistanceSensor::turnOff() {
  digitalWrite(loxSHT, LOW);  // Keep sensor off until initialized
  delay(10);  // Allow sensor to boot
}
bool DistanceSensor::begin(uint8_t loxAddress) {
    digitalWrite(loxSHT, HIGH);
    delay(10);  // Allow sensor to boot

    if (!lox.begin()) {  // Always starts at default address 0x29
        Serial.println("❌ Failed to begin VL53L0X at default 0x29");
        return false;
    }

    lox.setAddress(loxAddress);
    this->loxAddress = loxAddress;
    Serial.print("✅ VL53L0X assigned new address 0x");
    Serial.println(loxAddress, HEX);
    lox.startRangeContinuous();
    return true;
}

// bool DistanceSensor::begin(uint8_t loxAddress) {
//     // digitalWrite(loxSHT, HIGH);
//     // delay(10);  // Allow time for power-up

//     // if (!lox.begin(loxAddress)) {
//     //     Serial.print("❌ Failed to initialize VL53L0X at address 0x");
//     //     Serial.println(loxAddress, HEX);
//     //     return false;
//     // } else {
//     //     Serial.print("✅ VL53L0X initialized at address 0x");
//     //     Serial.println(loxAddress, HEX);
//     //     lox.startRangeContinuous();  // Start background measurement
//     //     return true;
//     // }
//     lox.startRangeContinuous();  // Start background measurement
//     return true;
// }
float DistanceSensor::readSensorMM() {
    return readSensor();
}
float DistanceSensor::readSensorCM() {
    float mm = readSensor();
    return (mm >= 0) ? mm / 10.0f : mm;
}
float DistanceSensor::readSensorM() {
      float mm = readSensor();
    return (mm >= 0) ? mm / 1000.0f : mm;
}

float DistanceSensor::readSensor() {
    // Use last measurement, even if still updating in background
    uint16_t distance = lox.readRange();
    if (lox.readRangeStatus() == VL53L0X_ERROR_NONE) {
        return static_cast<float>(distance);
    } else {
        return INVALID_SENSOR_READING;  // Invalid reading
    }
}
