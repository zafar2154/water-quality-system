#pragma once
#include <Arduino.h>
#include "CalibrationManager.h"

/**
 * TurbiditySensor
 * ──────────────────────────────────────────────────────────────────
 * Membaca dan mengkonversi sinyal analog sensor turbidity ke NTU.
 *
 *
 * Alur pakai:
 *   TurbiditySensor turb;
 *   turb.setCalibration(cal);         // muat kalibrasi dari Flash
 *   turb.update(ads.readVoltageMedian(ADS_Channel::TURBIDITY));
 *   float ntu = turb.getNTU();
 *
 * Kalibrasi:
 *   TurbidityCalData cal = CalibrationManager::calcTurbidity(v1, ntu1, v2, ntu2);
 *   calMgr.saveTurbidity(cal);
 *   turb.setCalibration(cal);
 *
 * Model konversi: NTU = slope * V + intercept  (regresi linear 2-titik)
 */
class TurbiditySensor
{
public:
    TurbiditySensor() = default;

    // ── Kalibrasi ─────────────────────────────────────────────────
    void             setCalibration(const TurbidityCalData &cal);
    TurbidityCalData getCalibration() const { return _cal; }
    bool             isCalibrated()   const { return _cal.valid; }

    // ── Update ────────────────────────────────────────────────────
    /**
     * Perbarui pembacaan dengan tegangan terbaru dari ADS1115.
     * @param voltage Tegangan dari ADS1115 channel TURBIDITY (V)
     */
    void update(float voltage);

    // ── Getter ────────────────────────────────────────────────────
    float getNTU()     const { return _ntu; }
    float getVoltage() const { return _voltage; }

private:
    TurbidityCalData _cal;
    float            _voltage = 0.0f;
    float            _ntu     = 0.0f;
};