#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

/**
 * ════════════════════════════════════════════════════════════════════
 *  FirebaseManager — Mengelola koneksi ke Firebase Realtime Database
 * ════════════════════════════════════════════════════════════════════
 *
 *  Fitur:
 *    - Koneksi anonim (tanpa login user) ke Firebase RTDB
 *    - Kirim data sensor ke path /readings/{timestamp}
 *    - Update status perangkat ke path /status
 *    - Cek status koneksi
 */

// ── Struct data sensor untuk dikirim ─────────────────────────────
struct SensorData
{
    String timestamp;    // "YYYY-MM-DD HH:MM:SS"
    float ph;
    float tds_ppm;
    float turbidity_ntu;
    float suhu_air;
    float suhu_udara;
    float wqi;
    const char *kategori;
};

class FirebaseManager
{
public:
    FirebaseManager() = default;

    /**
     * Inisialisasi Firebase dengan API Key, Database URL, Email, dan Password.
     * @param apiKey   Firebase Web API Key
     * @param dbUrl    Firebase Realtime Database URL
     * @param email    Email user terdaftar di Firebase Auth
     * @param password Password user
     */
    void begin(const char *apiKey, const char *dbUrl, const char *email, const char *password);

    /**
     * Kirim data sensor ke Firebase RTDB.
     * @param data   Struct SensorData berisi semua pembacaan
     * @param isSync Jika true, data langsung masuk ke history (tanpa update live)
     * @return true jika berhasil dikirim
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

    /** Buat key dari timestamp: "2026-06-30 14:51:07" → "2026-06-30_14-51-07" */
    String _makeKey(const String &timestamp);
};
