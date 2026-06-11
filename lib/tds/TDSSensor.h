#pragma once
#include <Arduino.h>
#include "CalibrationManager.h"

/**
 * TDSSensor
 * ──────────────────────────────────────────────────────────────────
 * Mengkonversi tegangan dari ADS1115 ke nilai TDS dalam PPM,
 * dengan kompensasi suhu dan koreksi kalibrasi K-Value.
 *
 * Alur pakai:
 *   TDSSensor tds;
 *   tds.setCalibration(cal);           // muat dari Flash
 *   tds.setTemperature(suhu);          // dari DS18B20
 *   tds.update(ads.readVoltageAvg(ADS_Channel::TDS));
 *   float ppm = tds.getTdsValue();
 *
 * Formula (DFRobot):
 *   Vcomp  = V / (1 + 0.02 * (T - 25))
 *   PPMraw = (133.42*Vcomp³ - 255.86*Vcomp² + 857.39*Vcomp) * 0.5
 *   PPMfin = PPMraw * kValue
 */
class TDSSensor
{
public:
    TDSSensor();

    // ── Kalibrasi ─────────────────────────────────────────────────
    void       setCalibration(const TdsCalData &cal);
    TdsCalData getCalibration() const { return _cal; }
    bool       isCalibrated()   const { return _cal.valid; }

    // ── Input ─────────────────────────────────────────────────────
    /**
     * Set suhu air aktual untuk kompensasi suhu.
     * @param tempC Suhu dalam °C (dari DS18B20 atau sumber lain)
     */
    void setTemperature(float tempC);

    /**
     * Perbarui perhitungan TDS dengan tegangan terbaru dari ADS1115.
     * @param voltage Tegangan dari ADS1115 channel TDS (V)
     */
    void update(float voltage);

    // ── Getter ────────────────────────────────────────────────────
    float getTdsValue() const { return _tdsValue; }
    float getVoltage()  const { return _voltage; }

private:
    TdsCalData _cal;
    float      _temperature = 25.0f; // default suhu kalibrasi standar
    float      _tdsValue    = 0.0f;
    float      _voltage     = 0.0f;
};