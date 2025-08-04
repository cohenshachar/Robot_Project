#ifndef DISTANCESENSOR_H
#define DISTANCESENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define INVALID_SENSOR_READING -1.0

class DistanceSensor {
public:
    // Constructor
    DistanceSensor(uint8_t loxSHT);

    // Initializes the sensor, returns true on success
    bool begin(uint8_t loxAddress);
    void turnOff();
    float readSensorMM();
    float readSensorCM();
    float readSensorM();
    uint8_t loxAddress;  // I2C address
    const uint8_t loxSHT;      // Shutdown pin (active-low)

    Adafruit_VL53L0X lox;                  // Sensor driver instance
private:
// Reads the last available distance measurement (in mm), returns -1 on failure
    float readSensor();

   // VL53L0X_RangingMeasurementData_t measure;  // Not strictly needed, but kept for future use
};

#endif
