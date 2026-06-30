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
 *    W → Set waktu RTC  (format: YYYY MM DD HH MM SS)
 *    Q → Baca tegangan mentah semua channel ADS1115
 *
 *  Kalibrasi:
 *    Edit koefisien langsung di header masing-masing sensor:
 *    - lib/turbidity/turbidity_sensor.h  (slope, intercept)
 *    - lib/tds/TDSSensor.h              (kValue)
 *    - lib/ph/PHSensor.h                (a, b, c)
 * ════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include "ADS.h"
#include "turbidity_sensor.h"
#include "TDSSensor.h"
#include "PHSensor.h"
#include "water_temp.h"
#include "RTC_DS3231_Wrapper.h"
#include "FuzzyWaterQuality.h"
#include <LiquidCrystal_I2C.h>

// ── Konfigurasi Pin ───────────────────────────────────────────────
constexpr uint8_t PIN_DS18B20 = 4; // GPIO4 untuk sensor suhu DS18B20

// ── Konfigurasi Interval Pembacaan ───────────────────────────────
constexpr uint32_t READ_INTERVAL_MS = 2000; // baca setiap 2 detik

// ── Interval tegangan mentah (mode Q) ─────────────────────────────
constexpr uint32_t RAW_VOLTAGE_INTERVAL_MS = 500; // tampilkan tiap 500ms

// ── Jumlah Sampel ADS1115 ─────────────────────────────────────────
constexpr uint8_t SAMPLES_AVG = 10;   // untuk TDS (rata-rata)
constexpr uint8_t SAMPLES_MEDIAN = 9; // untuk Turbidity & pH (median, lebih robust)

// ════════════════════════════════════════════════════════════════════
//  Instansiasi Objek
// ════════════════════════════════════════════════════════════════════

ADS modulADS;                       // ADS1115 (I2C 0x48)
TurbiditySensor sensorTurbidity;    // Turbidity  → A2
TDSSensor sensorTDS;                // TDS        → A1
PHSensor sensorPH;                  // pH         → A0
WaterTemp sensorSuhu(PIN_DS18B20);  // DS18B20 → GPIO4
RTC_DS3231_Wrapper rtc;             // DS3231 RTC (I2C 0x68) — waktu + suhu udara
FuzzyWaterQuality fuzzy;            // Fuzzy Logic Mamdani — kualitas air
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16x2 I2C (Address 0x27)

// ════════════════════════════════════════════════════════════════════
//  State untuk input Serial
// ════════════════════════════════════════════════════════════════════

enum class AppState
{
    NORMAL,       // Mode pembacaan normal
    RAW_VOLTAGE,  // Mode tegangan mentah terus-menerus (Q)
    SET_RTC_TIME, // Menunggu input waktu RTC
};

AppState state = AppState::NORMAL;

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
    Serial.println(F("  Perintah:"));
    Serial.println(F("  W → Set waktu RTC  (YYYY MM DD HH MM SS)"));
    Serial.println(F("  Q → Tegangan mentah semua channel ADS"));
    printSeparator('=');
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
    Serial.println(F("  ESP32 + ADS1115 + DS3231 | v3.0"));
    printSeparator('=');

    // ── Inisialisasi LCD ──────────────────────────────────────────
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Water Quality");
    lcd.setCursor(0, 1);
    lcd.print("System v3.0");

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

    // ── Tampilkan koefisien kalibrasi yang aktif ──────────────────
    Serial.println(F("\n[INIT] Koefisien kalibrasi (dari header sensor):"));

    TurbidityCalData tCal = sensorTurbidity.getCalibration();
    Serial.printf("  Turbidity : a=%.6f, b=%.6f, c=%.6f\n",
                  tCal.a, tCal.b, tCal.c);

    TdsCalData dCal = sensorTDS.getCalibration();
    Serial.printf("  TDS       : kValue=%.4f\n", dCal.kValue);

    PhCalData pCal = sensorPH.getCalibration();
    Serial.printf("  pH        : a=%.6f, b=%.6f, c=%.6f\n",
                  pCal.a, pCal.b, pCal.c);

    printMenu();
}

// ════════════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════════════

uint32_t lastReadTime = 0;
uint32_t lastRawVoltageTime = 0;

void loop()
{
    // ── Baca input Serial ────────────────────────────────────────
    if (Serial.available())
    {
        String input = readSerialLine();

        if (state == AppState::NORMAL)
        {
            if (input.equalsIgnoreCase("W"))
            {
                // Set waktu RTC secara manual
                Serial.println(F("\n[SET WAKTU RTC]"));
                Serial.println(F("  Masukkan waktu dalam format: YYYY MM DD HH MM SS"));
                Serial.println(F("  Contoh: 2026 06 23 18 30 00"));
                Serial.print(F("  > "));
                state = AppState::SET_RTC_TIME;
            }
            else if (input.equalsIgnoreCase("Q"))
            {
                // Masuk mode tegangan mentah terus-menerus
                Serial.println(F("\n  ── Mode Tegangan Mentah (LIVE) ──"));
                Serial.println(F("  Ketik apapun + Enter untuk berhenti.\n"));
                state = AppState::RAW_VOLTAGE;
                lastRawVoltageTime = 0; // paksa tampilkan segera
            }
            else if (input.length() > 0)
            {
                Serial.println(F("Perintah tidak dikenal. Ketik W/Q."));
            }
        }
        else if (state == AppState::RAW_VOLTAGE)
        {
            // Apapun yang diketik → keluar dari mode raw voltage
            Serial.println(F("\n  ── Mode Tegangan Mentah BERHENTI ──"));
            printMenu();
            state = AppState::NORMAL;
        }
        else if (state == AppState::SET_RTC_TIME)
        {
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
                    Serial.println(F("  Contoh: 2026 06 23 18 30 00"));
                }

                printMenu();
                state = AppState::NORMAL;
            }
        }
    }

    // ── Mode tegangan mentah terus-menerus ────────────────────────
    if (state == AppState::RAW_VOLTAGE &&
        (millis() - lastRawVoltageTime >= RAW_VOLTAGE_INTERVAL_MS))
    {
        lastRawVoltageTime = millis();

        float vPh = modulADS.readVoltage(ADS_Channel::PH);
        float vTds = modulADS.readVoltage(ADS_Channel::TDS);
        float vTurb = modulADS.readVoltage(ADS_Channel::TURBIDITY);
        float vSpare = modulADS.readVoltage(ADS_Channel::SPARE);

        Serial.printf("  Turb=%.4fV \n", vTurb);
    }

    // ── Pembacaan sensor periodik ─────────────────────────────────
    if (state == AppState::NORMAL &&
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

        // 6. Evaluasi kualitas air dengan Fuzzy Logic
        FuzzyResult wq = fuzzy.evaluate(ph, ppm, ntu, suhuAir, suhuUdara);

        // 7. Tampilkan hasil dengan timestamp
        printSeparator();

        // Timestamp dari RTC
        if (rtc.isReady())
        {
            DateTime now = rtc.getDateTime();
            Serial.printf("  %s, %s\n",
                          RTC_DS3231_Wrapper::getNamaHari(now),
                          rtc.getTimestamp().c_str());
        }

        Serial.printf("  Suhu Air   : %.1f \xb0"
                      "C  (DS18B20)\n",
                      suhuAir);

        if (suhuUdara > -900.0f)
            Serial.printf("  Suhu Udara : %.2f \xb0"
                          "C  (DS3231, delta Air-Udara=%+.1f\xb0"
                          "C)\n",
                          suhuUdara, suhuAir - suhuUdara);

        Serial.printf("  Turbidity  : %.1f NTU  (V=%.4fV)\n", ntu, vTurb);
        Serial.printf("  TDS        : %.0f PPM  (V=%.4fV)\n", ppm, vTds);
        Serial.printf("  pH         : %.2f (%s)  (V=%.4fV)\n", ph,
                      sensorPH.getPhCategory(), vPh);

        // Hasil Fuzzy Logic
        // printSeparator('-', 34);
        // Serial.printf("  Kualitas Air: %.1f / 100 [%s]\n", wq.wqi, wq.category);
        // printSeparator();

        // 8. Tampilkan ke LCD 16x2

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.printf("temp = %.2f", suhuAir);
        lcd.setCursor(0, 1);
        lcd.printf("v=%.3f, ph=%.2f", vPh, ph);
    }
}