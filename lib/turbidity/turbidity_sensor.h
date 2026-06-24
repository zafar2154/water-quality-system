#pragma once
#include <Arduino.h>

/**
 * TurbiditySensor
 * ──────────────────────────────────────────────────────────────────
 * Membaca dan mengkonversi sinyal analog sensor turbidity ke NTU.
 *
 * Model konversi: NTU = a*V² + b*V + c  (polinomial orde-2)
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║  KALIBRASI MANUAL                                               ║
 * ║  Ubah nilai slope dan intercept di struct TurbidityCalData       ║
 * ║  setelah melakukan kalibrasi, lalu upload ulang firmware.        ║
 * ║                                                                  ║
 * ║  Cara menghitung:                                                ║
 * ║  Gunakan data kalibrasi beberapa titik dan lakukan regresi       ║
 * ║  polinomial orde-2 (kuadratik) menggunakan Excel/Spreadsheet     ║
 * ║  untuk mendapatkan nilai a, b, dan c.                            ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * Alur pakai:
 *   TurbiditySensor turb;
 *   turb.update(ads.readVoltageMedian(ADS_Channel::TURBIDITY));
 *   float ntu = turb.getNTU();
 */

// ── Struct Kalibrasi ──────────────────────────────────────────────
struct TurbidityCalData
{
    // ┌──────────────────────────────────────────────────────────┐
    // │  EDIT NILAI DI BAWAH INI SETELAH KALIBRASI              │
    // │  NTU = a*V² + b*V + c                                   │
    // └──────────────────────────────────────────────────────────┘
    float a = 0.0f;         // koefisien V² (jika 0, rumus jadi linear)
    float b = -2222.22f;    // koefisien V
    float c = 3000.00f;     // konstanta
};

// ── Class Sensor ──────────────────────────────────────────────────
class TurbiditySensor
{
public:
    TurbiditySensor() = default;

    // ── Kalibrasi ─────────────────────────────────────────────────
    void setCalibration(const TurbidityCalData &cal);
    TurbidityCalData getCalibration() const { return _cal; }

    // ── Update ────────────────────────────────────────────────────
    /**
     * Perbarui pembacaan dengan tegangan terbaru dari ADS1115.
     * @param voltage Tegangan dari ADS1115 channel TURBIDITY (V)
     */
    void update(float voltage);

    // ── Getter ────────────────────────────────────────────────────
    float getNTU() const { return _ntu; }
    float getVoltage() const { return _voltage; }

private:
    TurbidityCalData _cal;
    float _voltage = 0.0f;
    float _ntu = 0.0f;
};