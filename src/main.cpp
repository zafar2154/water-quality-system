#include <Arduino.h>
#include "turbidity_sensor.h"

TurbiditySensor kekeruhanAir(34); // Pin analog untuk sensor turbidity

void setup()
{
  Serial.begin(115200);
  kekeruhanAir.begin();
}

void loop()
{
  // 2. Baca Kekeruhan
  float teganganKeruh = kekeruhanAir.readVoltage();
  float nilaiNTU = kekeruhanAir.readNTU();

  Serial.print("Voltase ADC : ");
  Serial.print(teganganKeruh);
  Serial.println(" V");

  Serial.print("Kekeruhan   : ");
  Serial.print(nilaiNTU);
  Serial.println(" NTU");

  delay(2000); // Tunggu 2 detik sebelum membaca lagi
}
