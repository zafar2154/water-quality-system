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
 *   A0 → pH (PH-4502C)    — GAIN_ONE  (±4.096V, resolusi 0.125 mV)
 *   A1 → TDS              — GAIN_ONE  (±4.096V, resolusi 0.125 mV)
 *   A2 → Turbidity        — GAIN_TWO  (±2.048V, resolusi 0.0625 mV)
 *   A3 → (reserved)       — GAIN_ONE  (default)
 *

 * SDA/SCL default: 21/22 (ESP32 DevKit)
 * VCC → 3.3V
 * GND → GND
 *
 * Metode pembacaan yang tersedia:
 *   readVoltage()       — Baca 1 sampel tunggal
 *   readVoltageAvg()    — Rata-rata N sampel (kurangi noise acak)
 *   readVoltageMedian() — Median N sampel (tahan terhadap spike/outlier)
 */

enum class ADS_Channel : uint8_t
{
    PH = 0,        // A0
    TDS = 1,       // A1
    TURBIDITY = 2, // A2
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
     * Set gain untuk channel tertentu. Panggil sebelum begin() atau
     * kapan saja untuk mengubah gain suatu channel.
     * @param ch   Channel yang ingin diubah gain-nya
     * @param gain Gain dari library Adafruit (GAIN_ONE, GAIN_TWO, dll.)
     */
    void setChannelGain(ADS_Channel ch, adsGain_t gain);

    /**
     * Baca tegangan 1 sampel dari satu kanal.
     * Gain otomatis disesuaikan per-channel sebelum konversi.
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

    /// Gain per-channel (default GAIN_ONE untuk semua)
    adsGain_t _channelGain[4] = {GAIN_ONE, GAIN_ONE, GAIN_ONE, GAIN_ONE};

    /// Gain yang saat ini aktif di hardware (untuk menghindari set ulang yang tidak perlu)
    adsGain_t _currentGain = GAIN_ONE;

    /**
     * Terapkan gain sesuai channel sebelum melakukan konversi ADC.
     * Hanya memanggil setGain() jika gain berbeda dari yang aktif,
     * untuk menghindari overhead I2C yang tidak perlu.
     */
    void applyGain(ADS_Channel ch);
};