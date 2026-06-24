#include "TDSSensor.h"

void TDSSensor::setCalibration(const TdsCalData &cal)
{
    _cal = cal;
    Serial.printf("[TDS] Kalibrasi di-set: kValue=%.4f\n", _cal.kValue);
}

void TDSSensor::setTemperature(float tempC)
{
    _temperature = tempC;
}

void TDSSensor::update(float voltage)
{
    _voltage = voltage;

    // Guard: tegangan tidak valid
    if (voltage < -900.0f)
    {
        _tdsValue = -1.0f;
        return;
    }

    // Deadband: jika tegangan di bawah 0.1V, probe di udara / tidak terhubung
    if (voltage < 0.1f)
    {
        _tdsValue = 0.0f;
        return;
    }

    // ── Kompensasi suhu (formula DFRobot) ─────────────────────────
    // Tegangan dikoreksi ke kondisi 25°C sebelum dihitung PPM
    float compensationCoeff   = 1.0f + 0.02f * (_temperature - 25.0f);
    float compensationVoltage = voltage / compensationCoeff;
    float Vc                  = compensationVoltage;

    // ── Hitung PPM mentah (polinomial orde-3 dari DFRobot) ────────
    float rawPpm = (133.42f * Vc * Vc * Vc
                  - 255.86f * Vc * Vc
                  + 857.39f * Vc) * 0.5f;

    // ── Terapkan kalibrasi K-Value ─────────────────────────────────
    _tdsValue = rawPpm * _cal.kValue;

    if (_tdsValue < 0.0f)
        _tdsValue = 0.0f;
}