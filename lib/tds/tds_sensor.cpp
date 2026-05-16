#include "tds_sensor.h"

TDSSensor::TDSSensor(uint8_t pin, float referensiTegangan) {
    analogPin = pin;
    aref = referensiTegangan;
}

void TDSSensor::begin() {
    pinMode(analogPin, INPUT);
    Serial.println("Sensor TDS diinisialisasi.");
}

float TDSSensor::readTDS(float suhuAirSekarang) {
    int arraySize = 50;
    int adcValues[50];

    // 1. Ambil data dengan kecepatan tinggi (dalam millivolt agar lebih presisi)
    for (int i = 0; i < arraySize; i++) {
        adcValues[i] = analogReadMilliVolts(analogPin); 
        delay(2);
    }

    // 2. Urutkan data (Bubble Sort)
    for (int i = 0; i < arraySize - 1; i++) {
        for (int j = 0; j < arraySize - i - 1; j++) {
            if (adcValues[j] > adcValues[j + 1]) {
                int temp = adcValues[j];
                adcValues[j] = adcValues[j + 1];
                adcValues[j + 1] = temp;
            }
        }
    }

    // 3. Trimmed Mean (Buang 10 atas, 10 bawah)
    long totalMilliVolts = 0;
    int count = 0;
    for (int i = 10; i < 40; i++) {
        totalMilliVolts += adcValues[i];
        count++;
    }

    // Hitung rata-rata Voltase
    float voltage = (totalMilliVolts / (float)count) / 1000.0;

    // 4. ALGORITMA KOMPENSASI SUHU (Standar Kalibrasi 25 Derajat Celcius)
    // Jika suhu sensor error (-127), kita asumsikan suhu standar 25 derajat agar TDS tetap bisa jalan
    if (suhuAirSekarang == -127.0) {
        suhuAirSekarang = 25.0; 
    }
    
    float compensationCoefficient = 1.0 + 0.02 * (suhuAirSekarang - 25.0);
    float compensationVoltage = voltage / compensationCoefficient;

    // 5. RUMUS KONVERSI TEGANGAN KE TDS (Rumus regresi standar modul TDS analog)
    float tdsValue = (133.42 * (compensationVoltage * compensationVoltage * compensationVoltage) 
                     - 255.86 * (compensationVoltage * compensationVoltage) 
                     + 857.39 * compensationVoltage) * 0.5;

    // Batasi nilai agar tidak negatif saat di air ultra-murni
    if (tdsValue < 0) {
        tdsValue = 0;
    }

    return tdsValue;
}