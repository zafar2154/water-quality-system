#include "SDManager.h"

// Constructor: Menyimpan pin CS saat object dibuat
SDManager::SDManager(uint8_t csPin) {
  _csPin = csPin;
  _isInitialized = false;
}

// Inisialisasi modul SD Card
bool SDManager::begin() {
  if (!SD.begin(_csPin)) {
    Serial.println("SD Card Mount Failed");
    return false;
  }
  _isInitialized = true;
  return true;
}

// Menampilkan informasi ukuran dan tipe SD Card
void SDManager::printCardInfo() {
  if (!_isInitialized) {
    Serial.println("System error: SD Card belum diinisialisasi.");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("Tidak ada SD card yang terpasang.");
    return;
  }

  Serial.print("Tipe SD Card: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SDHC");
  else Serial.println("UNKNOWN");

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("Kapasitas: %llu MB\n", cardSize);
}

// Menulis atau menimpa file teks
bool SDManager::writeText(const char* path, const char* message) {
  if (!_isInitialized) return false;
  
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Gagal membuka file untuk ditulis");
    return false;
  }
  
  if (file.print(message)) {
    file.close();
    return true;
  } else {
    Serial.println("Gagal menulis ke file");
    file.close();
    return false;
  }
}

// Membaca isi file teks
String SDManager::readText(const char* path) {
  if (!_isInitialized) return "";

  File file = SD.open(path);
  if (!file) {
    Serial.println("Gagal membuka file untuk dibaca");
    return "";
  }

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  
  file.close();
  return content;
}

// Menambahkan teks ke baris baru tanpa menghapus isi file sebelumnya
bool SDManager::appendText(const char* path, const char* message) {
  if (!_isInitialized) return false;
  
  // Perhatikan kita menggunakan FILE_APPEND di sini
  File file = SD.open(path, FILE_APPEND); 
  if (!file) {
    Serial.println("Gagal membuka file untuk ditambahkan (append)");
    return false;
  }
  
  // Gunakan println agar teks otomatis turun ke baris baru
  if (file.println(message)) {
    file.close();
    return true;
  } else {
    Serial.println("Gagal menambahkan teks ke file");
    file.close();
    return false;
  }
}