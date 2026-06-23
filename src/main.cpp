/**
 * ════════════════════════════════════════════════════════════════════
 *  Water Quality Monitoring System
 *  ESP32 + ADS1115 + DS3231 | Platform: PlatformIO | Framework: Arduino
 * ════════════════════════════════════════════════════════════════════
 *
 *  Sensor yang terhubung ke ADS1115 (I2C 0x48):
 *    A0 → pH Sensor PH-4502C (analog)
 *    A1 → TDS Sensor       (analog)
 *    A2 → Turbidity Sensor (analog)
 *
 *  Modul I2C lain:
 *    DS3231 RTC (I2C 0x68) — Waktu & suhu udara ambient
 *
 *  Sensor lain:
 *    GPIO4 → DS18B20 (suhu air, OneWire)
 *
 *  Perintah via Serial Monitor:
 *    T → Kalibrasi Turbidity  (2-titik)
 *    D → Kalibrasi TDS        (1-titik K-Value)
 *    P → Kalibrasi pH         (3-titik: buffer 4.01, 6.86, 9.18)
 *    R → Reset semua kalibrasi ke default
 *    W → Set waktu RTC       (format: YYYY MM DD HH MM SS)
 *    Q → Baca tegangan mentah semua channel ADS1115
 * ════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include "ADS.h"
#include "turbidity_sensor.h"
#include "TDSSensor.h"
#include "PHSensor.h"
#include "CalibrationManager.h"
#include "water_temp.h"
#include "RTC_DS3231_Wrapper.h"

// ── Konfigurasi Pin ───────────────────────────────────────────────
constexpr uint8_t PIN_DS18B20 = 4; // GPIO4 untuk sensor suhu DS18B20

// ── Konfigurasi Interval Pembacaan ───────────────────────────────
constexpr uint32_t READ_INTERVAL_MS = 500; // baca setiap 2 detik

// ── Interval live voltage saat kalibrasi ──────────────────────────
constexpr uint32_t CAL_LIVE_INTERVAL_MS = 500; // tampilkan tegangan tiap 1 detik

// ── Jumlah Sampel ADS1115 ─────────────────────────────────────────
constexpr uint8_t SAMPLES_AVG = 10;   // untuk TDS (rata-rata)
constexpr uint8_t SAMPLES_MEDIAN = 9; // untuk Turbidity & pH (median, lebih robust)

// ════════════════════════════════════════════════════════════════════
//  Instansiasi Objek
// ════════════════════════════════════════════════════════════════════

ADS modulADS;                      // ADS1115 (I2C 0x48)
TurbiditySensor sensorTurbidity;   // Turbidity  → A2
TDSSensor sensorTDS;               // TDS        → A1
PHSensor sensorPH;                 // pH         → A0
WaterTemp sensorSuhu(PIN_DS18B20); // DS18B20 → GPIO4
RTC_DS3231_Wrapper rtc;            // DS3231 RTC (I2C 0x68) — waktu + suhu udara
CalibrationManager calMgr;         // Manajemen kalibrasi + NVS Flash

// ════════════════════════════════════════════════════════════════════
//  State Machine Kalibrasi
// ════════════════════════════════════════════════════════════════════

enum class CalState
{
    NORMAL,               // Mode pembacaan normal
    CAL_TURB_WAIT_CLEAR,  // Turbidity: menunggu 'ok' di air jernih (live voltage)
    CAL_TURB_INPUT_NTU1,  // Turbidity: input NTU referensi titik 1
    CAL_TURB_WAIT_MURKY,  // Turbidity: menunggu 'ok' di air keruh  (live voltage)
    CAL_TURB_INPUT_NTU2,  // Turbidity: input NTU referensi titik 2
    CAL_TDS_WAIT_REF,     // TDS: menunggu 'ok' di larutan referensi (live voltage)
    CAL_TDS_INPUT_PPM,    // TDS: input PPM referensi
    CAL_PH_WAIT_BUF1,    // pH: menunggu 'ok' di buffer 4.01 (live voltage)
    CAL_PH_WAIT_BUF2,    // pH: menunggu 'ok' di buffer 6.86 (live voltage)
    CAL_PH_WAIT_BUF3,    // pH: menunggu 'ok' di buffer 9.18 (live voltage)
    SET_RTC_TIME,         // Set waktu RTC manual
};

CalState state = CalState::NORMAL;

// Variabel sementara selama proses kalibrasi
float calV1 = 0.0f, calV2 = 0.0f, calV3 = 0.0f;
float calNtu1 = 0.0f;  // NTU referensi titik 1 (untuk turbidity)
float calPpm = 0.0f;   // PPM mentah TDS saat kalibrasi

// Timer untuk live voltage display
uint32_t lastLiveVoltageTime = 0;

// ════════════════════════════════════════════════════════════════════
//  Fungsi Pembantu
// ════════════════════════════════════════════════════════════════════

/** Baca string dari Serial hingga newline, trim whitespace */
String readSerialLine()
{
    String s = Serial.readStringUntil('\n');
    s.trim();
    return s;
}

/** Cetak garis pemisah di Serial */
void printSeparator(char ch = '-', uint8_t len = 50)
{
    for (uint8_t i = 0; i < len; i++)
        Serial.print(ch);
    Serial.println();
}

/** Cetak menu perintah yang tersedia */
void printMenu()
{
    Serial.println();
    printSeparator('=');
    Serial.println(F("  Perintah Tersedia:"));
    Serial.println(F("  T → Kalibrasi Turbidity  (2-titik)"));
    Serial.println(F("  D → Kalibrasi TDS        (1-titik)"));
    Serial.println(F("  P → Kalibrasi pH         (3-titik)"));
    Serial.println(F("  R → Reset semua kalibrasi"));
    Serial.println(F("  W → Set waktu RTC        (YYYY MM DD HH MM SS)"));
    Serial.println(F("  Q → Tegangan mentah semua channel ADS"));
    printSeparator('=');
}

/** Baca tegangan dari ADS1115 dengan averaging (untuk konfirmasi kalibrasi) */
float bacaVoltageAvg(ADS_Channel ch)
{
    float v = modulADS.readVoltageAvg(ch, SAMPLES_AVG);
    Serial.printf("  [ADS] Channel %d: %.4f V (rata-rata %d sampel)\n",
                  (uint8_t)ch, v, SAMPLES_AVG);
    return v;
}

/** Baca tegangan dari ADS1115 dengan median (untuk konfirmasi kalibrasi) */
float bacaVoltageMedian(ADS_Channel ch)
{
    float v = modulADS.readVoltageMedian(ch, SAMPLES_MEDIAN);
    Serial.printf("  [ADS] Channel %d: %.4f V (median %d sampel)\n",
                  (uint8_t)ch, v, SAMPLES_MEDIAN);
    return v;
}

// ════════════════════════════════════════════════════════════════════
//  Live Voltage Display — Tampilkan tegangan realtime saat kalibrasi
// ════════════════════════════════════════════════════════════════════

/**
 * Menampilkan tegangan mentah sensor secara berkala (setiap 1 detik)
 * selama proses kalibrasi berlangsung, agar pengguna bisa melihat
 * kapan pembacaan sudah stabil sebelum mengetik 'ok'.
 */
void updateLiveVoltage()
{
    if (millis() - lastLiveVoltageTime < CAL_LIVE_INTERVAL_MS)
        return;

    lastLiveVoltageTime = millis();

    switch (state)
    {
    // ── Turbidity: tampilkan tegangan A2 ──────────────────────────
    case CalState::CAL_TURB_WAIT_CLEAR:
    case CalState::CAL_TURB_WAIT_MURKY:
    {
        float v = modulADS.readVoltage(ADS_Channel::TURBIDITY);
        Serial.printf("  📡 [LIVE] Turbidity (A2): %.4f V  |  Ketik 'ok' jika stabil\n", v);
        break;
    }

    // ── TDS: tampilkan tegangan A1 ────────────────────────────────
    case CalState::CAL_TDS_WAIT_REF:
    {
        float v = modulADS.readVoltage(ADS_Channel::TDS);
        Serial.printf("  📡 [LIVE] TDS (A1): %.4f V  |  Ketik 'ok' jika stabil\n", v);
        break;
    }

    // ── pH: tampilkan tegangan A0 ─────────────────────────────────
    case CalState::CAL_PH_WAIT_BUF1:
    case CalState::CAL_PH_WAIT_BUF2:
    case CalState::CAL_PH_WAIT_BUF3:
    {
        float v = modulADS.readVoltage(ADS_Channel::PH);
        Serial.printf("  📡 [LIVE] pH (A0): %.4f V  |  Ketik 'ok' jika stabil\n", v);
        break;
    }

    default:
        break;
    }
}

// ════════════════════════════════════════════════════════════════════
//  Handler State Machine Kalibrasi
// ════════════════════════════════════════════════════════════════════

void handleCalibration(const String &input)
{
    switch (state)
    {
    // ══════════════════════════════════════════════════════════════
    //  TURBIDITY — Kalibrasi 2-Titik
    // ══════════════════════════════════════════════════════════════

    case CalState::CAL_TURB_WAIT_CLEAR:
        if (input.equalsIgnoreCase("ok"))
        {
            // Ambil pembacaan final dengan median (lebih akurat)
            Serial.println(F("\n  ── Mengambil sampel final air jernih..."));
            calV1 = bacaVoltageMedian(ADS_Channel::TURBIDITY);
            Serial.printf("  ✔ Tegangan air jernih terkunci: %.4f V\n\n", calV1);

            Serial.println(F("  Langkah 2/4: Masukkan nilai NTU referensi air jernih."));
            Serial.println(F("  (Aquades ≈ 0 NTU, air PDAM ≈ 5 NTU)"));
            Serial.print(F("  NTU referensi titik 1: "));
            state = CalState::CAL_TURB_INPUT_NTU1;
        }
        break;

    case CalState::CAL_TURB_INPUT_NTU1:
        if (input.length() > 0)
        {
            calNtu1 = input.toFloat();
            Serial.printf("  NTU titik 1: %.2f\n", calNtu1);

            Serial.println(F("\n[KALIBRASI TURBIDITY] Langkah 3/4"));
            Serial.println(F("  Celupkan sensor ke AIR KERUH (contoh: larutan susu encer)."));
            Serial.println(F("  Tegangan live akan muncul di bawah."));
            Serial.println(F("  Ketik 'ok' lalu Enter saat tegangan sudah stabil.\n"));

            lastLiveVoltageTime = 0; // reset timer agar langsung tampil
            state = CalState::CAL_TURB_WAIT_MURKY;
        }
        break;

    case CalState::CAL_TURB_WAIT_MURKY:
        if (input.equalsIgnoreCase("ok"))
        {
            Serial.println(F("\n  ── Mengambil sampel final air keruh..."));
            calV2 = bacaVoltageMedian(ADS_Channel::TURBIDITY);
            Serial.printf("  ✔ Tegangan air keruh terkunci: %.4f V\n\n", calV2);

            Serial.println(F("  Langkah 4/4: Masukkan nilai NTU referensi air keruh."));
            Serial.print(F("  NTU referensi titik 2: "));
            state = CalState::CAL_TURB_INPUT_NTU2;
        }
        break;

    case CalState::CAL_TURB_INPUT_NTU2:
        if (input.length() > 0)
        {
            float ntu2 = input.toFloat();

            Serial.println(F("\n  ── Ringkasan Kalibrasi Turbidity ──"));
            Serial.printf("  Titik 1 (jernih): V = %.4f V → NTU = %.2f\n", calV1, calNtu1);
            Serial.printf("  Titik 2 (keruh) : V = %.4f V → NTU = %.2f\n", calV2, ntu2);

            TurbidityCalData cal = CalibrationManager::calcTurbidity(
                calV1, calNtu1, calV2, ntu2);

            if (cal.valid)
            {
                calMgr.saveTurbidity(cal);
                sensorTurbidity.setCalibration(cal);
                Serial.printf("  Koefisien: slope = %.4f, intercept = %.4f\n",
                              cal.slope, cal.intercept);
                Serial.println(F("  ✓ Kalibrasi Turbidity BERHASIL dan tersimpan!"));
            }
            else
            {
                Serial.println(F("  ✗ GAGAL! Pastikan tegangan kedua titik berbeda."));
            }

            printMenu();
            state = CalState::NORMAL;
        }
        break;

    // ══════════════════════════════════════════════════════════════
    //  TDS — Kalibrasi 1-Titik (K-Value)
    // ══════════════════════════════════════════════════════════════

    case CalState::CAL_TDS_WAIT_REF:
        if (input.equalsIgnoreCase("ok"))
        {
            // Ambil pembacaan final
            Serial.println(F("\n  ── Mengambil sampel final TDS..."));
            float v = bacaVoltageAvg(ADS_Channel::TDS);
            Serial.printf("  ✔ Tegangan TDS terkunci: %.4f V\n", v);

            float suhu = sensorSuhu.readTemperature();
            if (suhu < -100.0f)
                suhu = 25.0f; // fallback
            Serial.printf("  Suhu air saat kalibrasi: %.1f °C\n", suhu);

            TDSSensor tempTds;
            tempTds.setTemperature(suhu);
            tempTds.update(v);
            calPpm = tempTds.getTdsValue();

            Serial.printf("  PPM mentah (tanpa kalibrasi): %.2f PPM\n\n", calPpm);
            Serial.println(F("  Langkah 2/2: Masukkan nilai PPM referensi larutan."));
            Serial.print(F("  PPM referensi: "));
            state = CalState::CAL_TDS_INPUT_PPM;
        }
        break;

    case CalState::CAL_TDS_INPUT_PPM:
        if (input.length() > 0)
        {
            float refPpm = input.toFloat();

            Serial.println(F("\n  ── Ringkasan Kalibrasi TDS ──"));
            Serial.printf("  PPM mentah  : %.2f\n", calPpm);
            Serial.printf("  PPM referensi: %.2f\n", refPpm);

            TdsCalData cal = CalibrationManager::calcTds(calPpm, refPpm);

            if (cal.valid)
            {
                calMgr.saveTds(cal);
                sensorTDS.setCalibration(cal);
                Serial.printf("  K-Value: %.4f\n", cal.kValue);
                Serial.println(F("  ✓ Kalibrasi TDS BERHASIL dan tersimpan!"));
            }
            else
            {
                Serial.println(F("  ✗ GAGAL! PPM mentah terlalu kecil, cek probe."));
            }

            printMenu();
            state = CalState::NORMAL;
        }
        break;

    // ══════════════════════════════════════════════════════════════
    //  pH — Kalibrasi 3-Titik (Polinomial Orde-2)
    // ══════════════════════════════════════════════════════════════

    case CalState::CAL_PH_WAIT_BUF1:
        if (input.equalsIgnoreCase("ok"))
        {
            Serial.println(F("\n  ── Mengambil sampel final buffer pH 4.01..."));
            calV1 = bacaVoltageMedian(ADS_Channel::PH);
            Serial.printf("  ✔ V(pH 4.01) terkunci: %.4f V\n\n", calV1);

            Serial.println(F("[KALIBRASI pH] Langkah 2/3 — Buffer pH 6.86"));
            Serial.println(F("  Bilas elektroda dengan aquades, lalu celupkan ke buffer pH 6.86."));
            Serial.println(F("  Tegangan live akan muncul di bawah."));
            Serial.println(F("  Ketik 'ok' saat tegangan sudah stabil.\n"));

            lastLiveVoltageTime = 0;
            state = CalState::CAL_PH_WAIT_BUF2;
        }
        break;

    case CalState::CAL_PH_WAIT_BUF2:
        if (input.equalsIgnoreCase("ok"))
        {
            Serial.println(F("\n  ── Mengambil sampel final buffer pH 6.86..."));
            calV2 = bacaVoltageMedian(ADS_Channel::PH);
            Serial.printf("  ✔ V(pH 6.86) terkunci: %.4f V\n\n", calV2);

            Serial.println(F("[KALIBRASI pH] Langkah 3/3 — Buffer pH 9.18"));
            Serial.println(F("  Bilas elektroda, lalu celupkan ke buffer pH 9.18."));
            Serial.println(F("  Tegangan live akan muncul di bawah."));
            Serial.println(F("  Ketik 'ok' saat tegangan sudah stabil.\n"));

            lastLiveVoltageTime = 0;
            state = CalState::CAL_PH_WAIT_BUF3;
        }
        break;

    case CalState::CAL_PH_WAIT_BUF3:
        if (input.equalsIgnoreCase("ok"))
        {
            Serial.println(F("\n  ── Mengambil sampel final buffer pH 9.18..."));
            calV3 = bacaVoltageMedian(ADS_Channel::PH);
            Serial.printf("  ✔ V(pH 9.18) terkunci: %.4f V\n\n", calV3);

            Serial.println(F("  ── Ringkasan Kalibrasi pH ──"));
            Serial.printf("  Buffer pH 4.01 → %.4f V\n", calV1);
            Serial.printf("  Buffer pH 6.86 → %.4f V\n", calV2);
            Serial.printf("  Buffer pH 9.18 → %.4f V\n", calV3);

            Serial.println(F("\n  Menghitung koefisien polinomial (Gauss Elimination)..."));

            PhCalData cal = CalibrationManager::calcPh(
                calV1, 4.01f,
                calV2, 6.86f,
                calV3, 9.18f);

            if (cal.valid)
            {
                calMgr.savePh(cal);
                sensorPH.setCalibration(cal);
                Serial.printf("  Koefisien: a = %.6f, b = %.6f, c = %.6f\n",
                              cal.a, cal.b, cal.c);
                Serial.println(F("  ✓ Kalibrasi pH BERHASIL dan tersimpan!"));
            }
            else
            {
                Serial.println(F("  ✗ GAGAL! Pastikan ketiga tegangan buffer berbeda."));
            }

            printMenu();
            state = CalState::NORMAL;
        }
        break;

    // ══════════════════════════════════════════════════════════════
    //  SET WAKTU RTC
    // ══════════════════════════════════════════════════════════════

    case CalState::SET_RTC_TIME:
        if (input.length() > 0)
        {
            // Parse format: YYYY MM DD HH MM SS
            int yr, mo, dy, hr, mn, sc;
            int parsed = sscanf(input.c_str(), "%d %d %d %d %d %d",
                                &yr, &mo, &dy, &hr, &mn, &sc);

            if (parsed == 6 && yr >= 2000 && yr <= 2099 &&
                mo >= 1 && mo <= 12 && dy >= 1 && dy <= 31 &&
                hr >= 0 && hr <= 23 && mn >= 0 && mn <= 59 &&
                sc >= 0 && sc <= 59)
            {
                rtc.setDateTime(yr, mo, dy, hr, mn, sc);
                Serial.println(F("  ✓ Waktu RTC berhasil di-set!"));
                Serial.printf("  Waktu sekarang: %s\n", rtc.getTimestamp().c_str());
            }
            else
            {
                Serial.println(F("  ✗ Format salah! Gunakan: YYYY MM DD HH MM SS"));
                Serial.println(F("  Contoh: 2026 06 11 18 30 00"));
            }

            printMenu();
            state = CalState::NORMAL;
        }
        break;

    default:
        break;
    }
}

// ════════════════════════════════════════════════════════════════════
//  Masuk ke Mode Kalibrasi — Tampilkan instruksi awal
// ════════════════════════════════════════════════════════════════════

void startCalibration(CalState targetState)
{
    lastLiveVoltageTime = 0; // reset timer agar live voltage langsung tampil

    switch (targetState)
    {
    case CalState::CAL_TURB_WAIT_CLEAR:
        Serial.println(F("\n╔══════════════════════════════════════════════╗"));
        Serial.println(F("║       KALIBRASI TURBIDITY (2-TITIK)          ║"));
        Serial.println(F("╚══════════════════════════════════════════════╝"));
        Serial.println(F("\n  Langkah 1/4: Celupkan sensor ke AIR JERNIH (aquades)."));
        Serial.println(F("  Tegangan live akan muncul di bawah setiap 1 detik."));
        Serial.println(F("  Ketik 'ok' lalu Enter saat tegangan sudah stabil.\n"));
        state = CalState::CAL_TURB_WAIT_CLEAR;
        break;

    case CalState::CAL_TDS_WAIT_REF:
        Serial.println(F("\n╔══════════════════════════════════════════════╗"));
        Serial.println(F("║         KALIBRASI TDS (1-TITIK)              ║"));
        Serial.println(F("╚══════════════════════════════════════════════╝"));
        Serial.println(F("\n  Langkah 1/2: Celupkan probe TDS ke larutan REFERENSI PPM."));
        Serial.println(F("  (Contoh: larutan NaCl 1000 PPM atau air mineral botol)"));
        Serial.println(F("  Tegangan live akan muncul di bawah setiap 1 detik."));
        Serial.println(F("  Ketik 'ok' lalu Enter saat tegangan sudah stabil.\n"));
        state = CalState::CAL_TDS_WAIT_REF;
        break;

    case CalState::CAL_PH_WAIT_BUF1:
        Serial.println(F("\n╔══════════════════════════════════════════════╗"));
        Serial.println(F("║        KALIBRASI pH (3-TITIK)                ║"));
        Serial.println(F("╚══════════════════════════════════════════════╝"));
        Serial.println(F("\n  Langkah 1/3: Celupkan elektroda ke buffer pH 4.01."));
        Serial.println(F("  Tegangan live akan muncul di bawah setiap 1 detik."));
        Serial.println(F("  Ketik 'ok' lalu Enter saat tegangan sudah stabil.\n"));
        state = CalState::CAL_PH_WAIT_BUF1;
        break;

    default:
        break;
    }
}

// ════════════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════════════

void setup()
{
    Serial.begin(115200);
    delay(500);

    printSeparator('=');
    Serial.println(F("  Water Quality Monitoring System"));
    Serial.println(F("  ESP32 + ADS1115 + DS3231 | v2.2"));
    printSeparator('=');

    // ── Inisialisasi ADS1115 ──────────────────────────────────────
    if (!modulADS.begin())
    {
        Serial.println(F("[FATAL] ADS1115 tidak ditemukan! Periksa I2C."));
        while (1)
            delay(1000); // halt
    }

    // ── Inisialisasi RTC DS3231 ───────────────────────────────────
    if (!rtc.begin())
    {
        Serial.println(F("[WARN] DS3231 RTC tidak ditemukan. Waktu tidak tersedia."));
    }

    // ── Inisialisasi sensor suhu air ──────────────────────────────
    sensorSuhu.begin();

    // ── Muat kalibrasi dari NVS Flash ─────────────────────────────
    Serial.println(F("\n[INIT] Memuat kalibrasi dari Flash..."));
    sensorTurbidity.setCalibration(calMgr.loadTurbidity());
    sensorTDS.setCalibration(calMgr.loadTds());
    sensorPH.setCalibration(calMgr.loadPh());

    // ── Status kalibrasi ──────────────────────────────────────────
    Serial.println(F("\n[INIT] Status Kalibrasi:"));
    Serial.printf("  Turbidity : %s\n", sensorTurbidity.isCalibrated() ? "✓ Terkalibrasi" : "⚠ Default");
    Serial.printf("  TDS       : %s\n", sensorTDS.isCalibrated() ? "✓ Terkalibrasi" : "⚠ Default");
    Serial.printf("  pH        : %s\n", sensorPH.isCalibrated() ? "✓ Terkalibrasi" : "⚠ Default");

    printMenu();
}

// ════════════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════════════

uint32_t lastReadTime = 0;

void loop()
{
    // ── Live voltage display saat kalibrasi ────────────────────────
    if (state != CalState::NORMAL)
    {
        updateLiveVoltage();
    }

    // ── Baca input Serial ────────────────────────────────────────
    if (Serial.available())
    {
        String input = readSerialLine();

        if (state == CalState::NORMAL)
        {
            // Perintah di mode normal
            if (input.equalsIgnoreCase("T"))
            {
                startCalibration(CalState::CAL_TURB_WAIT_CLEAR);
            }
            else if (input.equalsIgnoreCase("D"))
            {
                startCalibration(CalState::CAL_TDS_WAIT_REF);
            }
            else if (input.equalsIgnoreCase("P"))
            {
                startCalibration(CalState::CAL_PH_WAIT_BUF1);
            }
            else if (input.equalsIgnoreCase("R"))
            {
                Serial.println(F("\n[RESET] Menghapus semua kalibrasi dari Flash..."));
                calMgr.resetTurbidity();
                calMgr.resetTds();
                calMgr.resetPh();
                // Muat ulang default
                sensorTurbidity.setCalibration(calMgr.loadTurbidity());
                sensorTDS.setCalibration(calMgr.loadTds());
                sensorPH.setCalibration(calMgr.loadPh());
                Serial.println(F("[RESET] Selesai. Semua sensor kembali ke kalibrasi default."));
                printMenu();
            }
            else if (input.equalsIgnoreCase("W"))
            {
                // Set waktu RTC secara manual
                Serial.println(F("\n[SET WAKTU RTC]"));
                Serial.println(F("  Masukkan waktu dalam format: YYYY MM DD HH MM SS"));
                Serial.println(F("  Contoh: 2026 06 11 18 30 00"));
                Serial.print(F("  > "));
                state = CalState::SET_RTC_TIME;
            }
            else if (input.equalsIgnoreCase("Q"))
            {
                // Baca tegangan mentah semua channel sekali
                Serial.println(F("\n  ── Tegangan Mentah Semua Channel ──"));
                Serial.printf("  A0 (pH)       : %.4f V\n", modulADS.readVoltage(ADS_Channel::PH));
                Serial.printf("  A1 (TDS)      : %.4f V\n", modulADS.readVoltage(ADS_Channel::TDS));
                Serial.printf("  A2 (Turbidity): %.4f V\n", modulADS.readVoltage(ADS_Channel::TURBIDITY));
                Serial.printf("  A3 (Spare)    : %.4f V\n", modulADS.readVoltage(ADS_Channel::SPARE));
                Serial.println();
            }
            else if (input.length() > 0)
            {
                Serial.println(F("Perintah tidak dikenal. Ketik T/D/P/R/W/Q."));
            }
        }
        else
        {
            // Proses input untuk state kalibrasi yang sedang berjalan
            handleCalibration(input);
        }
    }

    // ── Pembacaan sensor periodik (mode normal) ───────────────────
    if (state == CalState::NORMAL &&
        (millis() - lastReadTime >= READ_INTERVAL_MS))
    {
        lastReadTime = millis();

        // 1. Baca suhu air (DS18B20)
        float suhuAir = sensorSuhu.readTemperature();
        if (suhuAir < -100.0f)
            suhuAir = 25.0f; // fallback jika sensor tidak ada

        // 2. Baca suhu udara ambient (DS3231 internal)
        float suhuUdara = rtc.getTemperature();

        // 3. Update semua sensor dengan suhu air aktual
        sensorTDS.setTemperature(suhuAir);
        sensorPH.setTemperature(suhuAir);

        // 4. Baca tegangan dari ADS1115 dan update sensor
        float vTurb = modulADS.readVoltageMedian(ADS_Channel::TURBIDITY, SAMPLES_MEDIAN);
        float vTds = modulADS.readVoltageAvg(ADS_Channel::TDS, SAMPLES_AVG);
        float vPh = modulADS.readVoltageMedian(ADS_Channel::PH, SAMPLES_MEDIAN);

        sensorTurbidity.update(vTurb);
        sensorTDS.update(vTds);
        sensorPH.update(vPh);

        // 5. Ambil hasil pembacaan
        float ntu = sensorTurbidity.getNTU();
        float ppm = sensorTDS.getTdsValue();
        float ph = sensorPH.getPhValue();

        // 6. Tampilkan hasil dengan timestamp
        printSeparator();

        // Timestamp dari RTC
        if (rtc.isReady())
        {
            DateTime now = rtc.getDateTime();
            Serial.printf("  %s, %s\n",
                          RTC_DS3231_Wrapper::getNamaHari(now),
                          rtc.getTimestamp().c_str());
        }

        Serial.printf("  Suhu Air   : %.1f °C  (DS18B20)\n", suhuAir);

        if (suhuUdara > -900.0f)
            Serial.printf("  Suhu Udara : %.2f °C  (DS3231)\n", suhuUdara);

        Serial.printf("  Turbidity  : %.1f NTU  (V=%.4fV) %s\n", ntu, vTurb,
                      sensorTurbidity.isCalibrated() ? "" : "[default]");
        Serial.printf("  TDS        : %.0f PPM  (V=%.4fV) %s\n", ppm, vTds,
                      sensorTDS.isCalibrated() ? "" : "[default]");
        Serial.printf("  pH         : %.2f (%s)  (V=%.4fV) %s\n", ph,
                      sensorPH.getPhCategory(), vPh,
                      sensorPH.isCalibrated() ? "" : "[default]");
        printSeparator();
    }
}