#pragma once
#include <Arduino.h>

/**
 * FuzzyWaterQuality — Fuzzy Logic Mamdani untuk Penentuan Kualitas Air
 * ──────────────────────────────────────────────────────────────────
 *
 * Input (4 variabel):
 *   1. pH          (0–14)          → Asam, Netral, Basa
 *   2. TDS         (0–1500 PPM)    → Rendah, Sedang, Tinggi
 *   3. Turbidity   (0–1000 NTU)    → Jernih, Keruh
 *   4. Suhu Udara  (delta dari referensi, °C) → Dingin, Sedang, Panas
 *
 * Output:
 *   Water Quality Index (WQI) 0–100
 *   Kategori: Tidak Layak / Perlu Perlakuan / Layak
 *
 * Metode:
 *   - Fuzzifikasi   : Membership function trapesium/segitiga
 *   - Inferensi     : Mamdani (MIN untuk anteseden, MAX untuk agregasi)
 *   - Defuzzifikasi : Centroid (Center of Gravity)
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║  KONFIGURASI SUHU                                               ║
 * ║  Input suhu fuzzy = suhu_air_DS18B20 - suhu_udara_DS3231        ║
 * ║  Contoh: Jika air 25°C dan udara 30°C,                          ║
 * ║          maka delta = -5 → masuk "Dingin".                      ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

// ── Membership Function Trapesium ─────────────────────────────────
//
//       b _______ c
//      / |       | \
//     /  |       |  \
//    /   |       |   \
// __a____|_______|____d__
//
// Kasus khusus:
//   a == b : left shoulder  (μ=1 untuk x ≤ b)
//   c == d : right shoulder (μ=1 untuk x ≥ c)
//   b == c : segitiga (puncak tunggal)
//
struct TrapMF
{
    float a, b, c, d;

    /** Hitung derajat keanggotaan untuk nilai x */
    float evaluate(float x) const
    {
        if (x <= a) return (a == b) ? 1.0f : 0.0f;   // left shoulder
        if (x < b)  return (x - a) / (b - a);         // rising edge
        if (x <= c) return 1.0f;                       // plateau
        if (x < d)  return (d - x) / (d - c);         // falling edge
        return (c == d) ? 1.0f : 0.0f;                 // right shoulder
    }
};

// ── Struct Hasil Fuzzy ────────────────────────────────────────────
struct FuzzyResult
{
    float wqi;            // Water Quality Index (0–100)
    const char *category; // "Tidak Layak" / "Perlu Perlakuan" / "Layak"
};

// ── Class Utama ───────────────────────────────────────────────────
class FuzzyWaterQuality
{
public:
    /**
     * Evaluasi kualitas air menggunakan Fuzzy Logic Mamdani.
     *
     * @param ph         Nilai pH (0–14)
     * @param tds        Nilai TDS (PPM)
     * @param ntu        Nilai Turbidity (NTU)
     * @param suhuAir    Suhu air dari DS18B20 (°C)
     * @param suhuUdara  Suhu udara dari DS3231 (°C)
     * @return           FuzzyResult {wqi, category}
     */
    FuzzyResult evaluate(float ph, float tds, float ntu, float suhuAir, float suhuUdara);

    /**
     * Dapatkan kategori kualitas air berdasarkan WQI.
     */
    static const char *getCategory(float wqi);

private:
    // ── Input Membership Functions ────────────────────────────────
    static const TrapMF _phMF[3];   // 0=Asam, 1=Netral, 2=Basa
    static const TrapMF _tdsMF[3];  // 0=Rendah, 1=Sedang, 2=Tinggi
    static const TrapMF _turbMF[2]; // 0=Jernih, 1=Keruh
    static const TrapMF _suhuMF[3]; // 0=Dingin, 1=Sedang, 2=Panas

    // ── Output Membership Functions ───────────────────────────────
    static const TrapMF _outMF[3]; // 0=TidakLayak, 1=PerluPerlakuan, 2=Layak

    // ── Rule Table ────────────────────────────────────────────────
    // _rules[pH_idx][TDS_idx][Turb_idx][Suhu_idx] = output_idx
    // 0=TidakLayak, 1=PerluPerlakuan, 2=Layak
    static const uint8_t _rules[3][3][2][3];

    // ── Defuzzifikasi ─────────────────────────────────────────────
    static constexpr int CENTROID_POINTS = 200; // titik sampel untuk centroid
    static float _defuzzifyCentroid(const float outStrength[3]);
};
