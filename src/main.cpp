#include <Arduino.h>
#include "tds_sensor.h"

TDSSensor tdsSensor(35); // Pin analog untuk sensor TDS
float suhuSekarang = 25.0; // Asumsi suhu standar 25 derajat Celcius untuk kompensasi TDS
void setup()
{
  Serial.begin(115200);
  tdsSensor.begin();
}

void loop()
{
  // 2. Baca TDS
float nilaiTDS = tdsSensor.readTDS(suhuSekarang);
  Serial.print("Nilai TDS : ");
  Serial.print(nilaiTDS);
  Serial.println(" ppm");

  delay(1000); // Tunggu 2 detik sebelum membaca lagi
}
