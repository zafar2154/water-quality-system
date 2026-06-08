#ifndef ADS_H
#define ADS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

class ADS
{
private:
    Adafruit_ADS1115 ads; // Objek ADS1115 disembunyikan di sini (enkapsulasi)

public:
    // Fungsi untuk inisialisasi awal
    bool begin();

    // Fungsi untuk membaca tegangan berdasarkan channel (0-3)
    float readVoltage(uint8_t channel);
};

#endif