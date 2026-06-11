#include "turbidity_sensor.h"

void TurbiditySensor::setCalibration(const TurbidityCalData &cal)
{
    _cal = cal;
    Serial.printf("[Turbidity] Kalibrasi di-set: slope=%.4f, intercept=%.4f\n",
                  _cal.slope, _cal.intercept);
}

void TurbiditySensor::update(float voltage)
{
    _voltage = voltage;

    // Guard: tegangan tidak valid (ADS error atau sensor tidak terpasang)
    if (voltage < -900.0f)
    {
        _ntu = -1.0f; // indikator error
        return;
    }

    // Terapkan kalibrasi linear: NTU = slope * V + intercept
    _ntu = CalibrationManager::applyTurbidity(_voltage, _cal);
}