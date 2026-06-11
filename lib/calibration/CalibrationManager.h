#pragma once
#include <Arduino.h>
#include <Preferences.h> // NVS (Non-Volatile Storage) bawaan ESP32

// ═══════════════════════════════════════════════════════════════════
//  STRUKTUR DATA KALIBRASI — satu struct per jenis sensor
// ═══════════════════════════════════════════════════════════════════

/**
 * TurbidityCalData — Kalibrasi 2-titik, model regresi linear
 *
 *   NTU = slope * V + intercept
 *
 * Sensor        : Generic turbidity analog (terhubung ke ADS A0)
 * Titik kalibrasi: air jernih (NTU rendah) + air keruh (NTU tinggi)
 */
struct TurbidityCalData
{
    float slope     = -2222.22f; // default kasar — wajib dikalibrasi
    float intercept = 3000.00f;
    bool  valid     = false;     // true setelah dikalibrasi setidaknya 1x
};

// ───────────────────────────────────────────────────────────────────

/**
 * TdsCalData — Kalibrasi 1-titik, model K-Value (faktor koreksi)
 *
 *   PPM_final = PPM_raw * kValue
 *
 * Sensor        : TDS analog (terhubung ke ADS A1)
 * Titik kalibrasi: larutan referensi PPM yang diketahui nilainya
 */
struct TdsCalData
{
    float kValue = 1.0f; // default: tidak ada koreksi
    bool  valid  = false;
};

// ───────────────────────────────────────────────────────────────────

/**
 * PhCalData — Kalibrasi 3-titik, model polinomial orde-2
 *
 *   pH = a*V² + b*V + c
 *
 * Sensor        : PH-4502C (terhubung ke ADS A2)
 * Titik kalibrasi:
 *   - Buffer pH 4.01
 *   - Buffer pH 6.86
 *   - Buffer pH 9.18
 *
 * Koefisien dihitung otomatis dengan metode Gauss Elimination.
 */
struct PhCalData
{
    float a     = 0.0f;    // koefisien V²
    float b     = -3.5f;   // koefisien V  (default kasar)
    float c     = 21.34f;  // konstanta    (default kasar untuk 5V supply)
    bool  valid = false;
};

// ═══════════════════════════════════════════════════════════════════
//  CalibrationManager
// ═══════════════════════════════════════════════════════════════════

/**
 * CalibrationManager
 * ──────────────────
 * Mengelola perhitungan kalibrasi dan persistensi ke NVS Flash ESP32.
 * Data tersimpan permanen — tidak hilang meski ESP32 dimatikan.
 *
 * Cara pakai:
 *   CalibrationManager calMgr;
 *
 *   // Kalibrasi turbidity (2 titik):
 *   TurbidityCalData cal = CalibrationManager::calcTurbidity(v1,ntu1, v2,ntu2);
 *   calMgr.saveTurbidity(cal);
 *
 *   // Kalibrasi pH (3 titik):
 *   PhCalData calPh = CalibrationManager::calcPh(v1,4.01, v2,6.86, v3,9.18);
 *   calMgr.savePh(calPh);
 */
class CalibrationManager
{
public:
    // ── TURBIDITY ────────────────────────────────────────────────────

    /**
     * Hitung kalibrasi 2-titik untuk turbidity.
     * @param v1   Tegangan di air jernih (V)
     * @param ntu1 Nilai NTU referensi air jernih (biasanya 0)
     * @param v2   Tegangan di air keruh (V)
     * @param ntu2 Nilai NTU referensi air keruh
     * @return     TurbidityCalData siap pakai
     */
    static TurbidityCalData calcTurbidity(float v1, float ntu1,
                                          float v2, float ntu2);

    /**
     * Terapkan kalibrasi turbidity ke tegangan mentah.
     * @param voltage Tegangan dari ADS1115 (V)
     * @param cal     Data kalibrasi turbidity
     * @return        Nilai NTU terkalibasi (≥ 0)
     */
    static float applyTurbidity(float voltage, const TurbidityCalData &cal);

    bool             saveTurbidity(const TurbidityCalData &data);
    TurbidityCalData loadTurbidity();
    void             resetTurbidity();

    // ── TDS ──────────────────────────────────────────────────────────

    /**
     * Hitung K-Value dari satu titik kalibrasi.
     * @param measuredPpm PPM yang terbaca sensor (tanpa kalibrasi)
     * @param refPpm      PPM referensi larutan yang diketahui
     * @return            TdsCalData siap pakai
     */
    static TdsCalData calcTds(float measuredPpm, float refPpm);

    /**
     * Terapkan kalibrasi TDS ke PPM mentah.
     * @param rawPpm PPM dari rumus DFRobot (belum dikalibasi)
     * @param cal    Data kalibrasi TDS
     * @return       PPM final
     */
    static float applyTds(float rawPpm, const TdsCalData &cal);

    bool      saveTds(const TdsCalData &data);
    TdsCalData loadTds();
    void      resetTds();

    // ── pH ───────────────────────────────────────────────────────────

    /**
     * Hitung kalibrasi 3-titik untuk pH menggunakan Gauss Elimination.
     * Hasilkan koefisien polinomial orde-2:  pH = a*V² + b*V + c
     *
     * @param v1  Tegangan di buffer pH 4.01 (V)
     * @param v2  Tegangan di buffer pH 6.86 (V)
     * @param v3  Tegangan di buffer pH 9.18 (V)
     * @return    PhCalData siap pakai. valid=false jika gagal (singular)
     */
    static PhCalData calcPh(float v1, float ph1,
                            float v2, float ph2,
                            float v3, float ph3);

    /**
     * Terapkan kalibrasi pH ke tegangan mentah.
     * @param voltage Tegangan dari ADS1115 (V)
     * @param cal     Data kalibrasi pH
     * @return        Nilai pH (0.0–14.0), diclamp otomatis
     */
    static float applyPh(float voltage, const PhCalData &cal);

    bool     savePh(const PhCalData &data);
    PhCalData loadPh();
    void     resetPh();

private:
    Preferences _prefs;

    /**
     * Selesaikan sistem persamaan linear 3×3: A·x = b
     * Menggunakan Gauss Elimination dengan partial pivoting.
     * @return true jika berhasil (sistem tidak singular)
     */
    static bool _solve3x3(float A[3][3], float b[3], float x[3]);
};