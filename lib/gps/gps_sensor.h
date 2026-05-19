#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

class GPSSensor {
    private:
        HardwareSerial gpsSerial;
        TinyGPSPlus gps;
        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;

    public:
        // Constructor
        GPSSensor(uint8_t rx, uint8_t tx, uint32_t baud = 9600);
        
        // Fungsi Utama
        void begin();
        void update(); // Wajib diletakkan di loop() tanpa delay
        
        // Fungsi Pengambilan Data
        bool isFixed();
        double getLatitude();
        double getLongitude();
        int getSatellites();
        
        // Fungsi Debugging
        uint32_t getCharsProcessed();
};

#endif // GPS_SENSOR_H