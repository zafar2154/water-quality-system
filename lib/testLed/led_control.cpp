#include "led_control.h" // Wajib meng-include header-nya sendiri

// Isi dari fungsi setup LED
void setupLED() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    Serial.println("LED module telah diinisialisasi.");
}

// Isi dari fungsi kedip LED
void blinkLED(int delayTime) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("LED ON");
    delay(delayTime);
    
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("LED OFF");
    delay(delayTime);
}