#include <Arduino.h>
#include "ADS.h"       // Library ADS1115 buatan kita sebelumnya
#include "TDSSensor.h" // Library TDS OOP buatan kita

// Inisialisasi Objek (Instansiasi)
ADS modulADS;
TDSSensor sensorTDS;

void setup()
{
  Serial.begin(115200);

  if (!modulADS.begin())
  {
    Serial.println("Gagal menemukan ADS1115! Cek kabel I2C.");
    while (1)
      ;
  }

  Serial.println("Sistem Siap!");
}

void loop()
{
  // 1. Baca tegangan dari ADS1115 (Misal sensor TDS dicolok ke Channel A1)
  float voltTDS = modulADS.readVoltage(1);

  // 2. Baca suhu air aktual (Nanti diganti dengan kode pembacaan DS18B20)
  float suhuAirAktual = 31.0;

  // 3. Masukkan data suhu ke dalam objek TDS (Setter)
  sensorTDS.setTemperature(suhuAirAktual);

  // 4. Perintahkan objek TDS untuk memproses perhitungan dari tegangan (Update)
  sensorTDS.update(voltTDS);

  // 5. Ambil hasil perhitungan akhir (Getter)
  float nilaiPPM = sensorTDS.getTdsValue();

  // ==========================================
  // Tampilkan ke Serial Monitor
  // ==========================================
  Serial.print("Tegangan ADS : ");
  Serial.print(voltTDS, 3); // Cetak 3 angka di belakang koma
  Serial.print(" V  |  ");

  Serial.print("Suhu Air : ");
  Serial.print(suhuAirAktual, 1);
  Serial.print(" °C  |  ");

  Serial.print("Kualitas TDS : ");
  Serial.print(nilaiPPM, 0); // TDS biasanya dicetak tanpa angka desimal
  Serial.println(" PPM");

  delay(1500);
}