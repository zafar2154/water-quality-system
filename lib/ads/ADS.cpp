#include "ADS.h"

// Fungsi inisialisasi ADS1115
bool ADS::begin()
{
    // GAIN_ONE membaca rentang hingga +/- 4.096V (Aman untuk suplai 3.3V)
    ads.setGain(GAIN_ONE);

    // Data rate 8 SPS untuk pembacaan yang lebih stabil (bisa disesuaikan dengan kebutuhan)
    ads.setDataRate(RATE_ADS1115_8SPS);
    // Kembalikan status true jika berhasil, false jika gagal
    return ads.begin();
}

// Fungsi membaca tegangan
float ADS::readVoltage(uint8_t channel)
{
    // Validasi agar channel tidak melebihi 3 (A0, A1, A2, A3)
    if (channel > 3)
    {
        return 0.0;
    }

    // Ambil data mentah ADC lalu konversi ke Volt
    int16_t adc = ads.readADC_SingleEnded(channel);
    return ads.computeVolts(adc);
}