#include "TDSSensor.h"

// Constructor: Set nilai awal saat perangkat baru dinyalakan
TDSSensor::TDSSensor()
{
    _temperature = 25.0; // Suhu default standar kalibrasi
    _tdsValue = 0.0;
    _kValue = 1.0; // Nilai kalibrasi default (bisa disesuaikan)
}

// Implementasi Setter Suhu
void TDSSensor::setTemperature(float temp)
{
    _temperature = temp;
}

// Implementasi Setter K-Value
void TDSSensor::setKValue(float k)
{
    _kValue = k;
}

// Implementasi Update Rumus DFRobot
void TDSSensor::update(float voltage)
{
    // Filter Deadband untuk Noise di Udara
    // Jika tegangan di bawah 0.1 Volt, anggap probe sedang tidak di air
    if (voltage < 0.1)
    {
        _tdsValue = 0.0;
        return; // Hentikan kalkulasi di sini dan langsung keluar dari fungsi
    }

    // Jika tegangan normal (> 0.1V), lanjutkan kalkulasi
    // Kompensasi suhu: TDS mentah dibagi dengan faktor kompensasi berdasarkan suhu aktual (vcomp = V / (1 + 0.02 * (T - 25)))
    float compensationCoefficient = 1.0 + 0.02 * (_temperature - 25.0);
    float compensationVoltage = voltage / compensationCoefficient;

    // Hitung TDS Mentah (133.42 * V^3 - 255.86 * V^2 + 857.39 * V) * 0.5
    float rawTds = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

    // Kalibrasi hasil akhir dengan K-Value
    _tdsValue = rawTds * _kValue;

    // Pastikan nilai TDS tidak negatif (karena bisa terjadi noise atau kesalahan pembacaan)
    if (_tdsValue < 0)
    {
        _tdsValue = 0;
    }
}

// Implementasi Getter Nilai TDS
float TDSSensor::getTdsValue()
{
    return _tdsValue;
}