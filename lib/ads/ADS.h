#pragma once
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

/**
 * ADS1115
 * ──────────────────────────────────────────────────────────────────
 * Wrapper tunggal untuk ADS1115 yang dipakai bersama oleh semua
 * sensor analog (Turbidity, TDS, pH).
 *
 * Kanal (channel) ADS1115:
 *   A0 → Turbidity
 *   A1 → TDS
 *   A2 → pH (PH-4502C)
 *   A3 → (reserved)
 *
 * SDA/SCL default: 21/22 (ESP32 DevKit)
 * VCC → 3.3V
 * GND → GND
 *
 * Gain default: GAIN_ONE → range ±4.096 V, resolusi ~0.125 mV/bit
 *
 * Metode pembacaan yang tersedia:
 *   readVoltage()     — Baca 1 sampel tunggal
 *   readVoltageAvg()  — Rata-rata N sampel (kurangi noise acak)
 *   readVoltageMedian() — Median N sampel (tahan terhadap spike/outlier)
 */

enum class ADS_Channel : uint8_t
{
    TURBIDITY = 0, // A0
    TDS = 1,       // A1
    PH = 2,        // A2
    SPARE = 3      // A3
};

class ADS
{
public:
    explicit ADS(uint8_t i2cAddr = 0x48);

    /**
     * Inisialisasi ADS1115. Panggil di setup().
     * @return true jika modul ditemukan dan siap
     */
    bool begin();

    /**
     * Baca tegangan 1 sampel dari satu kanal.
     * @param ch  Kanal yang dibaca
     * @return    Tegangan (V), atau -999.0 jika ADS tidak siap
     */
    float readVoltage(ADS_Channel ch);

    /**
     * Baca rata-rata N sampel — efektif untuk noise acak (Gaussian).
     * @param ch      Kanal
     * @param samples Jumlah sampel (default 10, maks 64)
     * @return        Tegangan rata-rata (V)
     */
    float readVoltageAvg(ADS_Channel ch, uint8_t samples = 10);

    /**
     * Baca median N sampel — tahan terhadap spike / outlier.
     * Lebih lambat dari Avg, tapi lebih robust untuk sinyal noisy.
     * @param ch      Kanal
     * @param samples Jumlah sampel ganjil (default 9, maks 15)
     * @return        Tegangan median (V)
     */
    float readVoltageMedian(ADS_Channel ch, uint8_t samples = 9);

    bool isReady() const { return _ready; }

private:
    Adafruit_ADS1115 _ads;
    uint8_t _i2cAddr;
    bool _ready = false;
};