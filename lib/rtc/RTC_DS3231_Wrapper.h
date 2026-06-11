#pragma once
#include <Arduino.h>
#include <RTClib.h>

/**
 * RTC_DS3231_Wrapper — Wrapper OOP untuk modul RTC DS3231
 * ──────────────────────────────────────────────────────────────────
 * Menyediakan fungsi waktu/tanggal dan pembacaan suhu udara
 * dari sensor suhu internal DS3231 (akurasi ±3°C, resolusi 0.25°C).
 *
 * Koneksi I2C (berbagi bus dengan ADS1115):
 *   SDA → GPIO21
 *   SCL → GPIO22
 *   VCC → 3.3V (DS3231 mendukung 2.3V – 5.5V)
 *   GND → GND
 *   Alamat I2C: 0x68 (fixed, tidak bentrok dengan ADS1115 0x48)
 *
 * Fitur:
 *   - Waktu dan tanggal (jam, menit, detik, hari, bulan, tahun)
 *   - Suhu udara sekitar dari sensor internal DS3231
 *   - Sinkronisasi waktu otomatis saat kompilasi (jika RTC belum di-set)
 *   - Set waktu manual via Serial
 *
 * Alur pakai:
 *   RTC_DS3231_Wrapper rtc;
 *   rtc.begin();                        // inisialisasi di setup()
 *   DateTime now = rtc.getDateTime();   // ambil waktu sekarang
 *   float suhu  = rtc.getTemperature(); // suhu udara dari DS3231
 *   String ts   = rtc.getTimestamp();   // "2026-06-11 18:30:45"
 */
class RTC_DS3231_Wrapper
{
public:
    RTC_DS3231_Wrapper() = default;

    /**
     * Inisialisasi modul DS3231. Panggil di setup().
     * Jika RTC kehilangan daya (baterai habis), waktu akan
     * otomatis di-set ke waktu kompilasi firmware.
     * @return true jika DS3231 ditemukan dan siap
     */
    bool begin();

    /**
     * Cek apakah modul RTC berhasil diinisialisasi.
     * @return true jika RTC siap digunakan
     */
    bool isReady() const { return _ready; }

    // ── Waktu & Tanggal ───────────────────────────────────────────

    /**
     * Ambil waktu dan tanggal saat ini sebagai objek DateTime.
     * @return DateTime dari RTClib
     */
    DateTime getDateTime();

    /**
     * Ambil timestamp dalam format "YYYY-MM-DD HH:MM:SS".
     * Cocok untuk logging dan pencatatan data.
     * @return String timestamp terformat
     */
    String getTimestamp();

    /**
     * Ambil tanggal dalam format "DD/MM/YYYY".
     * @return String tanggal terformat
     */
    String getDateString();

    /**
     * Ambil waktu dalam format "HH:MM:SS".
     * @return String waktu terformat
     */
    String getTimeString();

    /**
     * Set waktu RTC secara manual.
     * @param year   Tahun penuh (misal 2026)
     * @param month  Bulan (1-12)
     * @param day    Hari (1-31)
     * @param hour   Jam (0-23)
     * @param minute Menit (0-59)
     * @param second Detik (0-59)
     */
    void setDateTime(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second);

    /**
     * Sinkronisasi waktu RTC dengan waktu kompilasi firmware.
     * Berguna jika RTC belum pernah di-set atau baterai baru diganti.
     */
    void syncToCompileTime();

    // ── Suhu Udara (Internal DS3231) ──────────────────────────────

    /**
     * Baca suhu udara sekitar dari sensor internal DS3231.
     *
     * Spesifikasi sensor suhu DS3231:
     *   - Rentang  : -40°C sampai +85°C
     *   - Akurasi  : ±3°C
     *   - Resolusi : 0.25°C (10-bit ADC internal)
     *   - Update   : otomatis setiap ~64 detik
     *
     * ⚠ CATATAN: Suhu ini adalah suhu udara di sekitar modul DS3231,
     *            BUKAN suhu air. Untuk suhu air gunakan DS18B20.
     *            Sensor ini cocok untuk monitoring suhu lingkungan/ambient.
     *
     * @return Suhu dalam °C, atau -999.0 jika RTC tidak siap
     */
    float getTemperature();

    /**
     * Nama hari dalam bahasa Indonesia (Senin, Selasa, dst.)
     * @param dt DateTime yang ingin diketahui nama harinya
     * @return Pointer ke string nama hari
     */
    static const char* getNamaHari(const DateTime &dt);

private:
    RTC_DS3231 _rtc;
    bool       _ready = false;
};
