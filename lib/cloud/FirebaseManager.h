#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

/**
 * ════════════════════════════════════════════════════════════════════
 *  FirebaseManager — Mengelola koneksi ke Firebase Realtime Database
 * ════════════════════════════════════════════════════════════════════
 *
 *  Struktur RTDB:
 *    /live_status  → data sensor saat ini (overwrite setiap upload)
 *    /sensor_history → riwayat data (push setiap 10x live update)
 */

// ── Struct data sensor untuk dikirim ─────────────────────────────
struct SensorData
{
    unsigned long timestamp; // Unix epoch (detik sejak 1 Jan 1970)
    float ph;
    float tds;
    float turbidity;
    float temperature;
    double lat;
    double lon;
};

class FirebaseManager
{
public:
    FirebaseManager() = default;

    /**
     * Inisialisasi Firebase dengan API Key, Database URL, Email, dan Password.
     */
    void begin(const char *apiKey, const char *dbUrl, const char *email, const char *password);

    /**
     * Kirim data sensor ke Firebase RTDB.
     * @param data   Struct SensorData
     * @param isSync Jika true, langsung push ke sensor_history (untuk offline sync)
     * @return true jika berhasil
     */
    bool sendReading(const SensorData &data, bool isSync = false);

    /**
     * Update status perangkat di Firebase.
     * Path: /status
     */
    void updateStatus();

    /** Cek apakah Firebase siap (token valid) */
    bool isReady() const;

private:
    FirebaseData _fbdo;
    FirebaseAuth _auth;
    FirebaseConfig _config;
    bool _initialized = false;
    uint8_t _liveCount = 0; // Penghitung untuk masuk ke sensor_history

    /** Build FirebaseJson dari SensorData (populate existing json) */
    void _buildJson(const SensorData &data, FirebaseJson &json);
};
