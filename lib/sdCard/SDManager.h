#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

class SDManager
{
private:
  uint8_t _csPin;
  bool _isInitialized;

public:
  // Constructor
  SDManager(uint8_t csPin);
  bool appendText(const char *path, const char *message);

  // Method dasar
  bool begin();
  void printCardInfo();

  // Method tambahan untuk Read & Write sebagai bonus OOP
  bool writeText(const char *path, const char *message);
  String readText(const char *path);
};

#endif