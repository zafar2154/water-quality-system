#ifndef TDS_SENSOR_H
#define TDS_SENSOR_H

#include <Arduino.h>

class TDSSensor {
  private:
    uint8_t analogPin;
    float aref; // Tegangan referensi ADC ESP32 (biasanya 3.3V)

  public:
    // Constructor
    TDSSensor(uint8_t pin, float referensiTegangan = 3.3);
    
    // Fungsi utama
    void begin();
    
    // Kita mewajibkan input suhu air saat ini untuk kompensasi TDS
    float readTDS(float suhuAirSekarang); 
};

#endif