#ifndef TDS_SENSOR_H
#define TDS_SENSOR_H

#include <Arduino.h>

class TDSSensor
{
private:
  float _temperature; // Variabel internal untuk menyimpan suhu
  float _tdsValue;    // Variabel internal untuk menyimpan hasil akhir PPM
  float _kValue;      // variabel konstanta untuk kalibrasi (bisa disesuaikan dengan kondisi nyata)

public:
  // Constructor (Dipanggil otomatis saat objek dibuat)
  TDSSensor();

  // Setter: Fungsi untuk memperbarui suhu air dari sensor DS18B20
  void setTemperature(float temp);
  void setKValue(float k); // Fungsi untuk memasukkan nilai kalibrasi

  // Fungsi utama: Memasukkan tegangan dari ADS1115 untuk dihitung menjadi PPM
  void update(float voltage);

  // Getter: Fungsi untuk mengambil hasil perhitungan TDS
  float getTdsValue();
};

#endif