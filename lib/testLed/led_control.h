#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <Arduino.h>

// definisikan pin LED internal
#define LED_BUILTIN 2

// fungsi untuk menginisialisasi pin LED
void setupLED();
void blinkLED(int delayTime);

#endif // LED_CONTROL_H