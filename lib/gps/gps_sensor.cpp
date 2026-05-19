#include "gps_sensor.h"

// Menggunakan Hardware Serial 1 ESP32
GPSSensor::GPSSensor(uint8_t rx, uint8_t tx, uint32_t baud) : gpsSerial(1) {
    rxPin = rx;
    txPin = tx;
    baudRate = baud;
}

void GPSSensor::begin() {
    gpsSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    Serial.println("Sensor GPS (Ublox NEO-M8N) siap digunakan.");
}

void GPSSensor::update() {
    // Membaca data secara terus menerus selama ada data yang masuk
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }
}

bool GPSSensor::isFixed() {
    return gps.location.isValid();
}

double GPSSensor::getLatitude() {
    if (isFixed()) return gps.location.lat();
    return 0.0;
}

double GPSSensor::getLongitude() {
    if (isFixed()) return gps.location.lng();
    return 0.0;
}

int GPSSensor::getSatellites() {
    if (gps.satellites.isValid()) return gps.satellites.value();
    return 0;
}

uint32_t GPSSensor::getCharsProcessed() {
    return gps.charsProcessed();
}