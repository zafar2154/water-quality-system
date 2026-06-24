#include "turbidity_sensor.h"

void TurbiditySensor::setCalibration(const TurbidityCalData &cal)
{
    _cal = cal;
    Serial.printf("[Turbidity] Kalibrasi di-set: a=%.6f, b=%.6f, c=%.6f\n",
                  _cal.a, _cal.b, _cal.c);
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

    // Terapkan kalibrasi polinomial orde-2: NTU = a*V² + b*V + c
    _ntu = _cal.a * voltage * voltage + _cal.b * voltage + _cal.c;

    // NTU tidak bisa negatif
    if (_ntu < 0.0f)
        _ntu = 0.0f;
}