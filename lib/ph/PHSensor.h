#pragma once
#include <Arduino.h>
#include "CalibrationManager.h"

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
 *   - Koneksi ke ADS1115 channel A2
 *
 * ⚠ PENTING: Class ini TIDAK membaca ADC langsung.
 *            Tegangan diambil dari ADS1115 oleh kode luar,
 *            lalu diumpankan melalui update(voltage).
 *
 * Alur pakai:
 *   PHSensor ph;
 *   ph.setCalibration(cal);            // muat dari Flash
 *   ph.setTemperature(suhu);           // opsional, kompensasi suhu
 *   ph.update(ads.readVoltageMedian(ADS_Channel::PH));
 *   float nilai = ph.getPhValue();
 *
 * Kalibrasi 3-titik:
 *   PhCalData cal = CalibrationManager::calcPh(v1,4.01, v2,6.86, v3,9.18);
 *   calMgr.savePh(cal);
 *   ph.setCalibration(cal);
 *
 * Model konversi: pH = a*V² + b*V + c  (polinomial orde-2)
 *
 * Kompensasi suhu (Nernst):
 *   Slope elektroda berubah linear dengan suhu absolut.
 *   Jika suhu sensor diketahui, kompensasi diterapkan secara software.
 */
class PHSensor
{
public:
    PHSensor() = default;

    // ── Kalibrasi ─────────────────────────────────────────────────
    void      setCalibration(const PhCalData &cal);
    PhCalData getCalibration() const { return _cal; }
    bool      isCalibrated()   const { return _cal.valid; }

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
     * Slope elektroda ∝ suhu absolut: factor = T_aktual / T_kalibrasi
     * @return faktor koreksi (dimensionless)
     */
    float _temperatureFactor() const;
};
