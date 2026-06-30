#pragma once
#include <Arduino.h>

/**
 * PHSensor — Sensor pH Analog (Modul PH-4502C)
 * ──────────────────────────────────────────────────────────────────
 * Mengkonversi tegangan dari ADS1115 ke nilai pH,
 * menggunakan kalibrasi 3-titik polinomial orde-2.
 *
 * Spesifikasi Modul PH-4502C:
 *   - Supply       : 5V DC
 *   - Output       : 0 – 3.4V analog
 *   - Rentang pH   : 0 – 14
 *   - Akurasi      : ±0.1 pH
 *   - Suhu kerja   : 0 – 60°C
 *   - Koneksi ke ADS1115 channel A0
 *
 * Model konversi: pH = a*V² + b*V + c  (polinomial orde-2)
 * Kompensasi suhu (Nernst): pH_final = 7 + (pH_raw - 7) / (T/298.15)
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║  KALIBRASI MANUAL                                               ║
 * ║  Ubah nilai a, b, c di struct PhCalData setelah kalibrasi.       ║
 * ║                                                                  ║
 * ║  Cara menghitung (3-titik):                                      ║
 * ║  1. Celupkan elektroda di buffer pH 4.01 → catat V1              ║
 * ║  2. Celupkan elektroda di buffer pH 6.86 → catat V2              ║
 * ║  3. Celupkan elektroda di buffer pH 9.18 → catat V3              ║
 * ║  4. Selesaikan sistem persamaan:                                  ║
 * ║     a*V1² + b*V1 + c = 4.01                                      ║
 * ║     a*V2² + b*V2 + c = 6.86                                      ║
 * ║     a*V3² + b*V3 + c = 9.18                                      ║
 * ║     (gunakan spreadsheet / kalkulator online)                     ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * Alur pakai:
 *   PHSensor ph;
 *   ph.setTemperature(suhu);           // opsional, kompensasi suhu
 *   ph.update(ads.readVoltageMedian(ADS_Channel::PH));
 *   float nilai = ph.getPhValue();
 */

// ── Struct Kalibrasi ──────────────────────────────────────────────
struct PhCalData
{
    // ┌──────────────────────────────────────────────────────────┐
    // │  EDIT NILAI DI BAWAH INI SETELAH KALIBRASI              │
    // │  pH = a*V² + b*V + c                                    │
    // └──────────────────────────────────────────────────────────┘
    float a = 0.4796f;    // koefisien V²  (default: tanpa lengkungan)
    float b = -9.041f;   // koefisien V   (default kasar)
    float c = 27.141f;  // konstanta     (default kasar untuk 5V supply)
};

// ── Class Sensor ──────────────────────────────────────────────────
class PHSensor
{
public:
    PHSensor() = default;

    // ── Kalibrasi ─────────────────────────────────────────────────
    void      setCalibration(const PhCalData &cal);
    PhCalData getCalibration() const { return _cal; }

    // ── Input ─────────────────────────────────────────────────────
    /**
     * Set suhu air untuk kompensasi suhu elektroda (opsional).
     * Default 25°C (suhu kalibrasi standar laboratorium).
     * @param tempC Suhu dalam °C
     */
    void setTemperature(float tempC);

    /**
     * Perbarui pembacaan dengan tegangan terbaru dari ADS1115.
     * @param voltage Tegangan dari ADS1115 channel PH (V)
     */
    void update(float voltage);

    // ── Getter ────────────────────────────────────────────────────
    float getPhValue()  const { return _phValue; }
    float getVoltage()  const { return _voltage; }

    /**
     * Interpretasi kualitas air berdasarkan nilai pH.
     * @return String deskripsi (mis. "Asam", "Netral", "Basa")
     */
    const char* getPhCategory() const;

private:
    PhCalData _cal;
    float     _temperature = 25.0f;
    float     _phValue     = 7.0f;  // default netral
    float     _voltage     = 0.0f;

    /**
     * Faktor koreksi suhu berdasarkan persamaan Nernst.
     * @return faktor koreksi (dimensionless)
     */
    float _temperatureFactor() const;
};
