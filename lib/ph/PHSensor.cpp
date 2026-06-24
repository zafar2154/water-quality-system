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

    // Hitung pH mentah: pH = a*V² + b*V + c
    float phRaw = _cal.a * voltage * voltage + _cal.b * voltage + _cal.c;

    // Clamp ke rentang fisik pH yang valid
    if (phRaw < 0.0f)  phRaw = 0.0f;
    if (phRaw > 14.0f) phRaw = 14.0f;

    // ── Kompensasi suhu elektroda (Nernst) ────────────────────────
    float factor = _temperatureFactor();
    _phValue = 7.0f + (phRaw - 7.0f) / factor;

    // Clamp final
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
