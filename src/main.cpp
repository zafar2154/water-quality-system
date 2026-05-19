#include <Arduino.h>
#include "tds_sensor.h"
#include "turbidity_sensor.h"
#include "water_temp.h"
#include "gps_sensor.h"

// Definisi Pin Sensor Air
#define PIN_TDS 35
#define PIN_TURBIDITY 34
#define PIN_TEMP 4

// Definisi Pin GPS
#define GPS_RX_PIN 16 // Hubungkan ke TX Modul GPS
#define GPS_TX_PIN 17 // Hubungkan ke RX Modul GPS
#define GPS_BAUD 9600 // Sesuaikan dengan hasil test yang berhasil sebelumnya

// Pembuatan Objek
TDSSensor tdsSensor(PIN_TDS);
TurbiditySensor turbiditySensor(PIN_TURBIDITY);
WaterTemp waterTempSensor(PIN_TEMP);
GPSSensor gpsSensor(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

// Pengatur Waktu Pembacaan Sensor
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 3000; // Print data setiap 3 detik

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi semua sensor
  waterTempSensor.begin();
  tdsSensor.begin();
  turbiditySensor.begin();
  gpsSensor.begin();
  
  Serial.println("\nSistem Pemantauan Kualitas Air & Lokasi Dimulai...\n");
}

void loop() {
  // PENTING: Fungsi update() ini harus dipanggil sesering mungkin.
  // Jangan menaruh fungsi delay() di manapun di dalam void loop()
  gpsSensor.update();

  // Timer Non-Blocking untuk print data sensor secara berkala
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();
    
    // Baca Sensor Kualitas Air
    float suhuSekarang = waterTempSensor.readTemperature();
    float nilaiTDS = tdsSensor.readTDS(suhuSekarang);
    float kekeruhan = turbiditySensor.readNTU();

    // Print Data ke Serial Monitor
    Serial.println("===================================");
    Serial.printf("Suhu Air    : %.2f °C\n", suhuSekarang);
    Serial.printf("Nilai TDS   : %.2f ppm\n", nilaiTDS);
    Serial.printf("Kekeruhan   : %.2f NTU\n", kekeruhan);
    Serial.println("-----------------------------------");

    // Print Data GPS
    if (gpsSensor.isFixed()) {
      Serial.println("Status GPS  : \xE2\x9C\x94 BERHASIL FIX");
      Serial.printf("Latitude    : %.6f\n", gpsSensor.getLatitude());
      Serial.printf("Longitude   : %.6f\n", gpsSensor.getLongitude());
      Serial.printf("Jml Satelit : %d\n", gpsSensor.getSatellites());
    } else {
      Serial.println("Status GPS  : \xE2\x8F\xB3 BELUM FIX (Mencari satelit...)");
      // Kita panggil getCharsProcessed() agar tahu alatnya tidak nge-hang saat mencari sinyal
      Serial.printf("Data Masuk  : %d karakter terbaca\n", gpsSensor.getCharsProcessed());
    }
    Serial.println("===================================\n");
  }
}