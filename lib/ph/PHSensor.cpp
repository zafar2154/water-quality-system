#include "PHSensor.h"

void PHSensor::setCalibration(const PhCalData &cal)
{
    _cal = cal;
    Serial.printf("[pH] Kalibrasi di-set: a=%.6f, b=%.6f, c=%.6f\n",
                  _cal.a, _cal.b, _cal.c);
}

void PHSensor::setTemperature(float tempC)
{
    _temperature = tempC;
}

float PHSensor::_temperatureFactor() const
{
    // Persamaan Nernst: slope elektroda berubah linear dengan suhu Kelvin
    // Slope_T = Slope_25 * (T_aktual_K / T_kalibrasi_K)
    // T_kalibrasi = 25°C = 298.15 K
    const float T_cal = 298.15f;
    float T_actual    = _temperature + 273.15f;
    return T_actual / T_cal;
}

void PHSensor::update(float voltage)
{
    _voltage = voltage;

    // Guard: tegangan tidak valid (ADS error)
    if (voltage < -900.0f)
    {
        _phValue = -1.0f;
        return;
    }

    // Hitung pH mentah menggunakan koefisien kalibrasi polinomial
    float phRaw = CalibrationManager::applyPh(_voltage, _cal);

    // ── Kompensasi suhu elektroda (Nernst) ────────────────────────
    // pH bergeser dari pH 7 (netral) proporsional terhadap suhu.
    // Koreksi: ph_terkoreksi = 7 + (phRaw - 7) / faktor_suhu
    //
    // Catatan: PH-4502C sudah punya kompensasi suhu hardware via NTC.
    //          Kompensasi software ini opsional dan lebih akurat
    //          jika nilai suhu aktual diketahui persis.
    float factor = _temperatureFactor();
    _phValue = 7.0f + (phRaw - 7.0f) / factor;

    // Clamp ke rentang fisik yang valid
    if (_phValue < 0.0f)  _phValue = 0.0f;
    if (_phValue > 14.0f) _phValue = 14.0f;
}

const char *PHSensor::getPhCategory() const
{
    if (_phValue < 0.0f)  return "ERROR";
    if (_phValue < 6.0f)  return "Asam";
    if (_phValue <= 8.0f) return "Netral";
    return "Basa";
}
