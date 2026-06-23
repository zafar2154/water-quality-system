#include "ADS.h"

ADS::ADS(uint8_t i2cAddr) : _i2cAddr(i2cAddr) {}

bool ADS::begin()
{
    // Set gain default untuk masing-masing channel
    _channelGain[static_cast<uint8_t>(ADS_Channel::PH)]        = GAIN_ONE; // ±4.096V
    _channelGain[static_cast<uint8_t>(ADS_Channel::TDS)]       = GAIN_ONE; // ±4.096V
    _channelGain[static_cast<uint8_t>(ADS_Channel::TURBIDITY)] = GAIN_TWO; // ±2.048V
    _channelGain[static_cast<uint8_t>(ADS_Channel::SPARE)]     = GAIN_ONE; // ±4.096V

    // Inisialisasi dengan GAIN_ONE sebagai default awal
    _ads.setGain(GAIN_ONE);
    _currentGain = GAIN_ONE;

    _ready = _ads.begin(_i2cAddr);

    if (!_ready)
    {
        Serial.println(F("[ADS1115] GAGAL inisialisasi! Periksa wiring I2C."));
    }
    else
    {
        Serial.printf("[ADS1115] OK — addr=0x%02X\n", _i2cAddr);
        Serial.println(F("[ADS1115] Gain per-channel:"));
        Serial.println(F("  A0 (pH)       : GAIN_ONE  (±4.096V, 0.125 mV/bit)"));
        Serial.println(F("  A1 (TDS)      : GAIN_ONE  (±4.096V, 0.125 mV/bit)"));
        Serial.println(F("  A2 (Turbidity): GAIN_TWO  (±2.048V, 0.0625 mV/bit)"));
    }

    return _ready;
}

void ADS::setChannelGain(ADS_Channel ch, adsGain_t gain)
{
    uint8_t idx = static_cast<uint8_t>(ch);
    if (idx < 4)
        _channelGain[idx] = gain;
}

void ADS::applyGain(ADS_Channel ch)
{
    uint8_t idx = static_cast<uint8_t>(ch);
    if (idx >= 4)
        return;

    adsGain_t targetGain = _channelGain[idx];

    // Hanya kirim perintah I2C jika gain berubah dari sebelumnya
    if (targetGain != _currentGain)
    {
        _ads.setGain(targetGain);
        _currentGain = targetGain;
    }
}

float ADS::readVoltage(ADS_Channel ch)
{
    if (!_ready)
        return -999.0f;

    applyGain(ch); // set gain sesuai channel sebelum baca
    int16_t raw = _ads.readADC_SingleEnded(static_cast<uint8_t>(ch));
    return _ads.computeVolts(raw);
}

float ADS::readVoltageAvg(ADS_Channel ch, uint8_t samples)
{
    if (!_ready || samples == 0)
        return -999.0f;
    if (samples > 64)
        samples = 64; // batas wajar

    float sum = 0.0f;
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += readVoltage(ch);
        delay(8); // jeda singkat agar ADS selesai konversi (~8.6ms di SPS 128)
    }
    return sum / samples;
}

float ADS::readVoltageMedian(ADS_Channel ch, uint8_t samples)
{
    if (!_ready || samples == 0)
        return -999.0f;
    if (samples > 15)
        samples = 15; // buffer statis max 15

    float buf[15];
    for (uint8_t i = 0; i < samples; i++)
    {
        buf[i] = readVoltage(ch);
        delay(8);
    }

    // Insertion sort (cepat untuk array kecil)
    for (uint8_t i = 1; i < samples; i++)
    {
        float key = buf[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && buf[j] > key)
        {
            buf[j + 1] = buf[j];
            j--;
        }
        buf[j + 1] = key;
    }

    // Ambil nilai tengah
    return buf[samples / 2];
}
