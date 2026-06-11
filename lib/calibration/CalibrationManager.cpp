#include "CalibrationManager.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════════════
//  PRIVATE HELPER — Gauss Elimination 3×3
// ═══════════════════════════════════════════════════════════════════

bool CalibrationManager::_solve3x3(float A[3][3], float b[3], float x[3])
{
    // Gauss Elimination dengan partial pivoting
    for (int col = 0; col < 3; col++)
    {
        // Cari baris pivot (nilai absolut terbesar di kolom ini)
        int pivotRow = col;
        for (int row = col + 1; row < 3; row++)
        {
            if (fabsf(A[row][col]) > fabsf(A[pivotRow][col]))
                pivotRow = row;
        }

        // Tukar baris pivot dengan baris saat ini
        if (pivotRow != col)
        {
            for (int k = 0; k < 3; k++)
            {
                float tmp = A[col][k];
                A[col][k] = A[pivotRow][k];
                A[pivotRow][k] = tmp;
            }
            float tmp = b[col];
            b[col]      = b[pivotRow];
            b[pivotRow] = tmp;
        }

        // Cek apakah sistem singular (pivot ≈ 0)
        if (fabsf(A[col][col]) < 1e-9f)
            return false;

        // Eliminasi ke bawah
        for (int row = col + 1; row < 3; row++)
        {
            float factor = A[row][col] / A[col][col];
            for (int k = col; k < 3; k++)
                A[row][k] -= factor * A[col][k];
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    for (int i = 2; i >= 0; i--)
    {
        x[i] = b[i];
        for (int j = i + 1; j < 3; j++)
            x[i] -= A[i][j] * x[j];
        x[i] /= A[i][i];
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  TURBIDITY — Kalibrasi 2-titik (regresi linear)
// ═══════════════════════════════════════════════════════════════════

TurbidityCalData CalibrationManager::calcTurbidity(float v1, float ntu1,
                                                   float v2, float ntu2)
{
    TurbidityCalData cal;

    // Cegah pembagian dengan nol jika kedua tegangan sama
    if (fabsf(v2 - v1) < 1e-6f)
    {
        Serial.println(F("[CalMgr] ERROR Turbidity: kedua tegangan identik!"));
        return cal; // valid tetap false
    }

    // Hitung slope dan intercept: NTU = slope * V + intercept
    cal.slope     = (ntu2 - ntu1) / (v2 - v1);
    cal.intercept = ntu1 - cal.slope * v1;
    cal.valid     = true;

    Serial.printf("[CalMgr] Turbidity cal: slope=%.4f, intercept=%.4f\n",
                  cal.slope, cal.intercept);
    return cal;
}

float CalibrationManager::applyTurbidity(float voltage,
                                         const TurbidityCalData &cal)
{
    float ntu = cal.slope * voltage + cal.intercept;
    return (ntu < 0.0f) ? 0.0f : ntu; // NTU tidak bisa negatif
}

bool CalibrationManager::saveTurbidity(const TurbidityCalData &data)
{
    bool ok = _prefs.begin("turb", false); // false = read-write
    if (!ok)
    {
        Serial.println(F("[CalMgr] ERROR: Gagal membuka NVS 'turb'"));
        return false;
    }
    _prefs.putFloat("slope",     data.slope);
    _prefs.putFloat("intercept", data.intercept);
    _prefs.putBool("valid",      data.valid);
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi Turbidity tersimpan ke Flash."));
    return true;
}

TurbidityCalData CalibrationManager::loadTurbidity()
{
    TurbidityCalData cal;
    if (!_prefs.begin("turb", true)) // true = read-only
        return cal;

    cal.slope     = _prefs.getFloat("slope",     cal.slope);
    cal.intercept = _prefs.getFloat("intercept", cal.intercept);
    cal.valid     = _prefs.getBool("valid",      false);
    _prefs.end();

    if (cal.valid)
        Serial.printf("[CalMgr] Turbidity dimuat: slope=%.4f, intercept=%.4f\n",
                      cal.slope, cal.intercept);
    else
        Serial.println(F("[CalMgr] Turbidity: belum dikalibrasi, pakai default."));
    return cal;
}

void CalibrationManager::resetTurbidity()
{
    _prefs.begin("turb", false);
    _prefs.clear();
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi Turbidity direset."));
}

// ═══════════════════════════════════════════════════════════════════
//  TDS — Kalibrasi 1-titik (K-Value)
// ═══════════════════════════════════════════════════════════════════

TdsCalData CalibrationManager::calcTds(float measuredPpm, float refPpm)
{
    TdsCalData cal;

    if (measuredPpm < 1.0f)
    {
        Serial.println(F("[CalMgr] ERROR TDS: PPM terukur terlalu kecil!"));
        return cal;
    }

    cal.kValue = refPpm / measuredPpm;
    cal.valid  = true;

    Serial.printf("[CalMgr] TDS cal: kValue=%.4f\n", cal.kValue);
    return cal;
}

float CalibrationManager::applyTds(float rawPpm, const TdsCalData &cal)
{
    float ppm = rawPpm * cal.kValue;
    return (ppm < 0.0f) ? 0.0f : ppm;
}

bool CalibrationManager::saveTds(const TdsCalData &data)
{
    bool ok = _prefs.begin("tds", false);
    if (!ok)
    {
        Serial.println(F("[CalMgr] ERROR: Gagal membuka NVS 'tds'"));
        return false;
    }
    _prefs.putFloat("kvalue", data.kValue);
    _prefs.putBool("valid",   data.valid);
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi TDS tersimpan ke Flash."));
    return true;
}

TdsCalData CalibrationManager::loadTds()
{
    TdsCalData cal;
    if (!_prefs.begin("tds", true))
        return cal;

    cal.kValue = _prefs.getFloat("kvalue", cal.kValue);
    cal.valid  = _prefs.getBool("valid",   false);
    _prefs.end();

    if (cal.valid)
        Serial.printf("[CalMgr] TDS dimuat: kValue=%.4f\n", cal.kValue);
    else
        Serial.println(F("[CalMgr] TDS: belum dikalibrasi, pakai default."));
    return cal;
}

void CalibrationManager::resetTds()
{
    _prefs.begin("tds", false);
    _prefs.clear();
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi TDS direset."));
}

// ═══════════════════════════════════════════════════════════════════
//  pH — Kalibrasi 3-titik (polinomial orde-2: pH = a*V² + b*V + c)
// ═══════════════════════════════════════════════════════════════════

PhCalData CalibrationManager::calcPh(float v1, float ph1,
                                     float v2, float ph2,
                                     float v3, float ph3)
{
    PhCalData cal;

    // Susun sistem persamaan linear:
    // [v1² v1 1] [a]   [ph1]
    // [v2² v2 1] [b] = [ph2]
    // [v3² v3 1] [c]   [ph3]
    float A[3][3] = {
        {v1 * v1, v1, 1.0f},
        {v2 * v2, v2, 1.0f},
        {v3 * v3, v3, 1.0f}
    };
    float b[3] = {ph1, ph2, ph3};
    float x[3] = {0.0f, 0.0f, 0.0f};

    if (!_solve3x3(A, b, x))
    {
        Serial.println(F("[CalMgr] ERROR pH: sistem singular, cek tegangan buffer!"));
        return cal; // valid tetap false
    }

    cal.a     = x[0];
    cal.b     = x[1];
    cal.c     = x[2];
    cal.valid = true;

    Serial.printf("[CalMgr] pH cal: a=%.6f, b=%.6f, c=%.6f\n",
                  cal.a, cal.b, cal.c);
    return cal;
}

float CalibrationManager::applyPh(float voltage, const PhCalData &cal)
{
    float ph = cal.a * voltage * voltage + cal.b * voltage + cal.c;
    // Clamp ke rentang fisik pH yang valid
    if (ph < 0.0f)  ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

bool CalibrationManager::savePh(const PhCalData &data)
{
    bool ok = _prefs.begin("ph", false);
    if (!ok)
    {
        Serial.println(F("[CalMgr] ERROR: Gagal membuka NVS 'ph'"));
        return false;
    }
    _prefs.putFloat("a",     data.a);
    _prefs.putFloat("b",     data.b);
    _prefs.putFloat("c",     data.c);
    _prefs.putBool("valid",  data.valid);
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi pH tersimpan ke Flash."));
    return true;
}

PhCalData CalibrationManager::loadPh()
{
    PhCalData cal;
    if (!_prefs.begin("ph", true))
        return cal;

    cal.a     = _prefs.getFloat("a",    cal.a);
    cal.b     = _prefs.getFloat("b",    cal.b);
    cal.c     = _prefs.getFloat("c",    cal.c);
    cal.valid = _prefs.getBool("valid", false);
    _prefs.end();

    if (cal.valid)
        Serial.printf("[CalMgr] pH dimuat: a=%.6f, b=%.6f, c=%.6f\n",
                      cal.a, cal.b, cal.c);
    else
        Serial.println(F("[CalMgr] pH: belum dikalibrasi, pakai default."));
    return cal;
}

void CalibrationManager::resetPh()
{
    _prefs.begin("ph", false);
    _prefs.clear();
    _prefs.end();
    Serial.println(F("[CalMgr] Kalibrasi pH direset."));
}
