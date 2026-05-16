#include "turbidity_sensor.h"

TurbiditySensor::TurbiditySensor(uint8_t pin)
{
    analogPin = pin;
}

void TurbiditySensor::begin()
{
    pinMode(analogPin, INPUT);
    Serial.println("Turbidity Sensor siap digunakan.");
}

float TurbiditySensor::readVoltage()
{
    // 1. Buat variabel untuk menampung total jumlah pembacaan
    long totalADC = 0;
    int jumlahSampel = 100; // Kita ambil 100 sampel data

    // 2. Lakukan perulangan cepat untuk membaca sensor 100 kali
    for (int i = 0; i < jumlahSampel; i++)
    {
        totalADC += analogRead(analogPin);
        delay(2); // Beri jeda 2 milidetik agar ADC ESP32 bisa "bernapas"
    }

    // 3. Hitung rata-rata ADC-nya
    float averageADC = (float)totalADC / jumlahSampel;

    // 4. Baru konversi ke Voltase
    float voltage = averageADC * (3.3 / 4095.0);
    return voltage;
}

float TurbiditySensor::readNTU()
{
    float voltage = readVoltage();

    // Rumus estimasi kasar konversi Tegangan ke NTU
    // Air jernih = Voltase tinggi (~3.0V - 3.3V) = NTU mendekati 0
    // Air keruh = Voltase rendah (< 2.5V) = NTU tinggi
    float airJernihVoltage = 1.35; // sesuaikan dengan kondisi air jernih di lingkungan Anda
    float airKeruhVoltage = 0;     // sesuaikan dengan kondisi air keruh di lingkungan Anda

    // Batasi tegangan agar tidak melebihi rentang kalibrasi kita
    if (voltage > airJernihVoltage)
    {
        voltage = airJernihVoltage;
    }
    else if (voltage < airKeruhVoltage)
    {
        voltage = airKeruhVoltage;
    }

    // Terapkan rumus regresi linier: y = -2222.22x + 3000
    float ntu = (-2222.22 * voltage) + 3000.0;

    // Pastikan NTU tidak pernah bernilai negatif akibat pembulatan
    if (ntu < 0)
    {
        ntu = 0;
    }

    return ntu;
}