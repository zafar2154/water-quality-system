#include "RTC_DS3231_Wrapper.h"

// ── Nama hari dalam Bahasa Indonesia ─────────────────────────────
static const char *NAMA_HARI[] = {
    "Minggu", "Senin", "Selasa", "Rabu",
    "Kamis", "Jumat", "Sabtu"};

bool RTC_DS3231_Wrapper::begin()
{
    if (!_rtc.begin())
    {
        Serial.println(F("[RTC] DS3231 tidak ditemukan! Periksa koneksi I2C."));
        _ready = false;
        return false;
    }

    _ready = true;

    // Cek apakah RTC kehilangan daya (oscillator berhenti)
    if (_rtc.lostPower())
    {
        Serial.println(F("[RTC] ⚠ Baterai RTC habis / pertama kali dinyalakan."));
        Serial.println(F("[RTC] Menyinkronkan waktu ke waktu kompilasi firmware..."));
        syncToCompileTime();
    }

    // Tampilkan waktu saat ini
    DateTime now = _rtc.now();
    Serial.printf("[RTC] OK — %s, %s\n",
                  getNamaHari(now),
                  getTimestamp().c_str());

    // Tampilkan suhu modul
    float temp = getTemperature();
    Serial.printf("[RTC] Suhu modul DS3231: %.2f °C\n", temp);

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  Waktu & Tanggal
// ═══════════════════════════════════════════════════════════════════

DateTime RTC_DS3231_Wrapper::getDateTime()
{
    if (!_ready)
        return DateTime((uint32_t)0);

    return _rtc.now();
}

String RTC_DS3231_Wrapper::getTimestamp()
{
    if (!_ready)
        return F("RTC ERROR");

    DateTime now = _rtc.now();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    return String(buf);
}

String RTC_DS3231_Wrapper::getDateString()
{
    if (!_ready)
        return F("RTC ERROR");

    DateTime now = _rtc.now();
    char buf[11];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             now.day(), now.month(), now.year());
    return String(buf);
}

String RTC_DS3231_Wrapper::getTimeString()
{
    if (!_ready)
        return F("RTC ERROR");

    DateTime now = _rtc.now();
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());
    return String(buf);
}

void RTC_DS3231_Wrapper::setDateTime(uint16_t year, uint8_t month, uint8_t day,
                                     uint8_t hour, uint8_t minute, uint8_t second)
{
    if (!_ready)
    {
        Serial.println(F("[RTC] ERROR: RTC tidak siap, tidak bisa set waktu."));
        return;
    }

    _rtc.adjust(DateTime(year, month, day, hour, minute, second));

    Serial.printf("[RTC] Waktu di-set ke: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);
}

void RTC_DS3231_Wrapper::syncToCompileTime()
{
    if (!_ready)
        return;

    // Macro __DATE__ dan __TIME__ berisi waktu saat firmware dikompilasi
    _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println(F("[RTC] Waktu disinkronkan ke waktu kompilasi."));
}

// ═══════════════════════════════════════════════════════════════════
//  Suhu Udara (Internal DS3231)
// ═══════════════════════════════════════════════════════════════════

float RTC_DS3231_Wrapper::getTemperature()
{
    if (!_ready)
        return -999.0f;

    // DS3231 memperbarui suhu internal setiap ~64 detik secara otomatis.
    // Method getTemperature() membaca register suhu tanpa memaksa konversi baru.
    return _rtc.getTemperature();
}

// ═══════════════════════════════════════════════════════════════════
//  Utilitas
// ═══════════════════════════════════════════════════════════════════

const char *RTC_DS3231_Wrapper::getNamaHari(const DateTime &dt)
{
    // dayOfTheWeek() mengembalikan 0=Minggu, 1=Senin, ... 6=Sabtu
    uint8_t dow = dt.dayOfTheWeek();
    if (dow > 6)
        dow = 0;
    return NAMA_HARI[dow];
}
