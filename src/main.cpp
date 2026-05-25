#include <Arduino.h>
#include "SDManager.h"

#define CS_PIN 5
SDManager mySD(CS_PIN);

void setup()
{
  Serial.begin(115200);
  delay(2000);

  if (mySD.begin())
  {
    Serial.println("SD Card OK!");
    Serial.println("Silakan ketik sesuatu di Serial Monitor lalu tekan Enter.");
  }
  else
  {
    Serial.println("Gagal membaca SD Card.");
  }
}

void loop()
{
  // Mengecek apakah ada data yang dikirim dari Serial Monitor
  if (Serial.available() > 0)
  {

    // Membaca teks sampai kamu menekan tombol Enter (\n)
    String inputUser = Serial.readStringUntil('\n');

    // Membersihkan karakter spasi atau enter berlebih di awal/akhir teks
    inputUser.trim();

    // Pastikan yang dikirim bukan teks kosong
    if (inputUser.length() > 0)
    {
      Serial.print("Menyimpan ke SD Card: ");
      Serial.println(inputUser);

      // Simpan teks dari Serial Monitor ke dalam file "catatan_serial.txt"
      // Ingat: .c_str() digunakan untuk mengubah String menjadi const char*
      if (mySD.appendText("/catatan_serial.txt", inputUser.c_str()))
      {
        Serial.println("-> Berhasil disimpan!\n");
      }
      else
      {
        Serial.println("-> Gagal menyimpan!\n");
      }
    }
  }
}