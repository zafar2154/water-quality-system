#include "water_temp.h"

// Implementasi Constructor
// Sintaks " : oneWire(pin), sensors(&oneWire)" digunakan untuk memasukkan nilai ke instance sebelum class selesai dibuat
WaterTemp::WaterTemp(uint8_t pin) : oneWire(pin), sensors(&oneWire) {}

void WaterTemp::begin() {
    sensors.begin();
    Serial.println("Test Sensor Suhu DS18B20 dimulai...");
}

float WaterTemp::readTemperature() {
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    
    if (temperatureC == DEVICE_DISCONNECTED_C) {
        Serial.println("Error: Sensor water temperature tidak terdeteksi! Cek kabel dan resistor 4.7k.");
        return -127.0; // Return -127.0 untuk menandakan error
    } 
    return temperatureC;
}