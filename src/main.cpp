#include <Arduino.h>
#include "water_temp.h"

WaterTemp suhuAir(4); // Pin data sensor DS18B20

void setup()
{
  Serial.begin(115200);
  suhuAir.begin();
}

void loop()
{
  float suhuSekarang = suhuAir.readTemperature();
  if (suhuSekarang != -127.0) { // Cek apakah pembacaan suhu berhasil
    Serial.print("Suhu Air: ");
    Serial.print(suhuSekarang);
    Serial.println(" ºC");
  }
  delay(2000); // Baca suhu setiap 2 detik
}
