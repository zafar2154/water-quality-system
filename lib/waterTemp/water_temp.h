#ifndef WATER_TEMP_H
#define WATER_TEMP_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class WaterTemp {
    private:
        OneWire oneWire;
        DallasTemperature sensors;
    public:
    // Constructor: Fungsi khusus yang dipanggil saat objek dibuat
        WaterTemp(uint8_t pin);

        // Fungsi utama
        void begin();
        float readTemperature();
};

#endif // WATER_TEMP_H