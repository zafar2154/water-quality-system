#include "FuzzyWaterQuality.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════════════
//  MEMBERSHIP FUNCTIONS — INPUT
// ═══════════════════════════════════════════════════════════════════

// ── pH ────────────────────────────────────────────────────────────
// Asam   : trapesium (0, 0, 6, 7)     — left shoulder, penuh di pH ≤ 6
// Netral : segitiga  (6.5, 7, 8.5)    — puncak di pH 7
// Basa   : trapesium (7, 9, 14, 14)   — right shoulder, penuh di pH ≥ 9
const TrapMF FuzzyWaterQuality::_phMF[3] = {
    {0.0f, 0.0f, 6.0f, 7.0f},     // Asam
    {6.5f, 7.0f, 7.0f, 8.5f},     // Netral (segitiga)
    {7.0f, 9.0f, 14.0f, 14.0f},   // Basa
};

// ── TDS (PPM) ─────────────────────────────────────────────────────
// Rendah : trapesium (0, 0, 300, 500)   — left shoulder, air bersih
// Sedang : segitiga  (300, 600, 1000)   — puncak di 600 PPM
// Tinggi : segitiga  (600, 1000, 1000)  — right shoulder dari 600 ke atas
const TrapMF FuzzyWaterQuality::_tdsMF[3] = {
    {0.0f, 0.0f, 300.0f, 500.0f},     // Rendah
    {300.0f, 600.0f, 600.0f, 1000.0f}, // Sedang (segitiga)
    {600.0f, 1000.0f, 1000.0f, 1000.0f}, // Tinggi (right shoulder)
};

// ── Turbidity (NTU) ───────────────────────────────────────────────
// Jernih : trapesium (0, 0, 1, 5)        — left shoulder, penuh di ≤ 1 NTU
// Keruh  : trapesium (0, 1, 5, 1000)     — naik dari 0, plateau 1–5, turun perlahan
const TrapMF FuzzyWaterQuality::_turbMF[2] = {
    {0.0f, 0.0f, 1.0f, 5.0f},       // Jernih
    {0.0f, 1.0f, 5.0f, 1000.0f},    // Keruh
};

// ── Suhu (delta = suhu_udara - SUHU_REFERENSI) ───────────────────
// Dingin : trapesium (-100, -100, -5, -3) — left shoulder, delta sangat negatif
// Sedang : trapesium (-5, -3, 3, 5)       — di sekitar 0 (suhu ideal)
// Panas  : trapesium (3, 5, 100, 100)     — right shoulder, delta sangat positif
const TrapMF FuzzyWaterQuality::_suhuMF[3] = {
    {-100.0f, -100.0f, -5.0f, -3.0f}, // Dingin
    {-5.0f, -3.0f, 3.0f, 5.0f},       // Sedang
    {3.0f, 5.0f, 100.0f, 100.0f},     // Panas
};

// ═══════════════════════════════════════════════════════════════════
//  MEMBERSHIP FUNCTIONS — OUTPUT
// ═══════════════════════════════════════════════════════════════════

// Tidak Layak      : trapesium (0, 0, 30, 50)    — left shoulder
// Perlu Perlakuan  : segitiga  (30, 50, 70)       — puncak di 50
// Layak            : trapesium (50, 70, 100, 100) — right shoulder
const TrapMF FuzzyWaterQuality::_outMF[3] = {
    {0.0f, 0.0f, 30.0f, 50.0f},     // Tidak Layak
    {30.0f, 50.0f, 50.0f, 70.0f},   // Perlu Perlakuan (segitiga)
    {50.0f, 70.0f, 100.0f, 100.0f}, // Layak
};

// ═══════════════════════════════════════════════════════════════════
//  RULE TABLE — 54 aturan (3 pH × 3 TDS × 2 Turb × 3 Suhu)
// ═══════════════════════════════════════════════════════════════════
//
// Indeks:
//   pH   : 0=Asam, 1=Netral, 2=Basa
//   TDS  : 0=Rendah, 1=Sedang, 2=Tinggi
//   Turb : 0=Jernih, 1=Keruh
//   Suhu : 0=Dingin, 1=Sedang, 2=Panas
//
// Output:
//   0 = Tidak Layak  (TL)
//   1 = Perlu Perlakuan (PP)
//   2 = Layak (L)
//
// Prinsip aturan:
//   1. pH Asam atau Basa          → Tidak Layak (selalu)
//   2. Turbidity Keruh            → Tidak Layak (selalu)
//   3. TDS Tinggi                 → Tidak Layak
//   4. TDS Sedang / Suhu abnormal → Perlu Perlakuan
//   5. Semua normal               → Layak
//
const uint8_t FuzzyWaterQuality::_rules[3][3][2][3] = {
    // ── pH = Asam (0) ──────────────────────────────────────────
    // Semua kombinasi → Tidak Layak
    {
        // TDS=Rendah    Turb=Jernih{D,S,P}  Turb=Keruh{D,S,P}
        /*TDS=Rendah*/ {{0, 0, 0},           {0, 0, 0}},
        /*TDS=Sedang*/ {{0, 0, 0},           {0, 0, 0}},
        /*TDS=Tinggi*/ {{0, 0, 0},           {0, 0, 0}},
    },

    // ── pH = Netral (1) ────────────────────────────────────────
    {
        //              Turb=Jernih{D,S,P}   Turb=Keruh{D,S,P}
        /*TDS=Rendah*/ {{1, 2, 1},           {0, 0, 0}},
        /*TDS=Sedang*/ {{1, 1, 1},           {0, 0, 0}},
        /*TDS=Tinggi*/ {{0, 0, 0},           {0, 0, 0}},
    },

    // ── pH = Basa (2) ──────────────────────────────────────────
    // Semua kombinasi → Tidak Layak
    {
        //              Turb=Jernih{D,S,P}   Turb=Keruh{D,S,P}
        /*TDS=Rendah*/ {{0, 0, 0},           {0, 0, 0}},
        /*TDS=Sedang*/ {{0, 0, 0},           {0, 0, 0}},
        /*TDS=Tinggi*/ {{0, 0, 0},           {0, 0, 0}},
    },
};

// ═══════════════════════════════════════════════════════════════════
//  EVALUASI FUZZY
// ═══════════════════════════════════════════════════════════════════

FuzzyResult FuzzyWaterQuality::evaluate(float ph, float tds, float ntu,
                                         float suhuAir, float suhuUdara)
{
    // ── 1. Hitung delta suhu (suhu air - suhu udara) ──────────────
    float deltaTemp = suhuAir - suhuUdara;

    // ── 2. Fuzzifikasi — hitung derajat keanggotaan semua input ──
    float phMu[3], tdsMu[3], turbMu[2], suhuMu[3];

    for (int i = 0; i < 3; i++) phMu[i]   = _phMF[i].evaluate(ph);
    for (int i = 0; i < 3; i++) tdsMu[i]  = _tdsMF[i].evaluate(tds);
    for (int i = 0; i < 2; i++) turbMu[i] = _turbMF[i].evaluate(ntu);
    for (int i = 0; i < 3; i++) suhuMu[i] = _suhuMF[i].evaluate(deltaTemp);

    // ── 3. Evaluasi semua 54 aturan (Mamdani) ────────────────────
    // Untuk setiap aturan:
    //   firing_strength = MIN(μ_pH, μ_TDS, μ_Turb, μ_Suhu)
    // Untuk setiap output class:
    //   outStrength = MAX dari firing_strength semua aturan yang menuju class tsb
    float outStrength[3] = {0.0f, 0.0f, 0.0f};

    for (uint8_t ip = 0; ip < 3; ip++)       // pH
    {
        if (phMu[ip] < 0.001f) continue; // skip jika membership ≈ 0

        for (uint8_t it = 0; it < 3; it++)   // TDS
        {
            if (tdsMu[it] < 0.001f) continue;

            for (uint8_t iu = 0; iu < 2; iu++)  // Turb
            {
                if (turbMu[iu] < 0.001f) continue;

                for (uint8_t is = 0; is < 3; is++) // Suhu
                {
                    if (suhuMu[is] < 0.001f) continue;

                    // Firing strength = MIN dari semua anteseden
                    float strength = phMu[ip];
                    if (tdsMu[it]  < strength) strength = tdsMu[it];
                    if (turbMu[iu] < strength) strength = turbMu[iu];
                    if (suhuMu[is] < strength) strength = suhuMu[is];

                    // Ambil output class dari rule table
                    uint8_t outIdx = _rules[ip][it][iu][is];

                    // Agregasi: MAX
                    if (strength > outStrength[outIdx])
                        outStrength[outIdx] = strength;
                }
            }
        }
    }

    // ── 4. Defuzzifikasi Centroid ──────────────────────────────────
    float wqi = _defuzzifyCentroid(outStrength);

    FuzzyResult result;
    result.wqi      = wqi;
    result.category = getCategory(wqi);
    return result;
}

// ═══════════════════════════════════════════════════════════════════
//  DEFUZZIFIKASI CENTROID
// ═══════════════════════════════════════════════════════════════════

float FuzzyWaterQuality::_defuzzifyCentroid(const float outStrength[3])
{
    float numerator   = 0.0f;
    float denominator = 0.0f;

    for (int i = 0; i <= CENTROID_POINTS; i++)
    {
        float x = 100.0f * i / CENTROID_POINTS; // titik sampel [0, 100]

        // Hitung membership teragregasi di titik x
        // Agregasi = MAX dari semua output MF yang di-clip (MIN dengan outStrength)
        float muAgg = 0.0f;
        for (int j = 0; j < 3; j++)
        {
            float mfVal  = _outMF[j].evaluate(x);
            float clipped = (mfVal < outStrength[j]) ? mfVal : outStrength[j];
            if (clipped > muAgg) muAgg = clipped;
        }

        numerator   += x * muAgg;
        denominator += muAgg;
    }

    // Jika tidak ada aturan yang aktif, kembalikan 50 (tengah)
    if (denominator < 0.0001f)
        return 50.0f;

    return numerator / denominator;
}

// ═══════════════════════════════════════════════════════════════════
//  KATEGORI
// ═══════════════════════════════════════════════════════════════════

const char *FuzzyWaterQuality::getCategory(float wqi)
{
    if (wqi < 40.0f) return "Tidak Layak";
    if (wqi < 60.0f) return "Perlu Perlakuan";
    return "Layak";
}
