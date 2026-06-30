#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "FirebaseManager.h"
#include "SDManager.h"
#include "CloudConfig.h"

/**
 * ════════════════════════════════════════════════════════════════════
 *  DataLogger — Orkestrator penyimpanan data (Online + Offline)
 * ════════════════════════════════════════════════════════════════════
 *
 *  Alur:
 *    1. Selalu simpan ke SD Card sebagai log CSV harian
 *       → /data/YYYY-MM-DD.csv
 *    2. Jika WiFi terhubung:
 *       → Kirim ke Firebase RTDB
 *       → Cek & sync data pending dari SD Card
 *    3. Jika WiFi tidak terhubung:
 *       → Tambahkan ke antrian pending → /pending.csv
 *       → Akan di-sync otomatis saat WiFi kembali
 */

class DataLogger
{
public:
    DataLogger() = default;

    /**
     * Inisialisasi WiFi, SD Card, dan Firebase.
     * Dipanggil sekali di setup().
     */
    void begin();

    /**
     * Log data sensor: simpan ke SD, kirim ke Firebase jika online.
     * @param data  Struct SensorData berisi semua pembacaan
     */
    void log(const SensorData &data);

    /**
     * Periksa koneksi WiFi dan reconnect jika perlu.
     * Panggil secara periodik di loop().
     */
    void maintain();

    // ── Status ────────────────────────────────────────────────────
    bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
    bool isFirebaseReady() const { return _firebase.isReady(); }
    bool isSDReady()       const { return _sdReady; }

    /** Jumlah data pending di SD Card yang belum di-sync */
    uint16_t getPendingCount() const { return _pendingCount; }

private:
    FirebaseManager _firebase;
    SDManager       _sd{SDCARD_CS_PIN};
    bool            _sdReady = false;
    uint16_t        _pendingCount = 0;
    uint32_t        _lastReconnectAttempt = 0;

    // ── WiFi ─────────────────────────────────────────────────────
    void _connectWiFi();

    // ── SD Card ──────────────────────────────────────────────────
    /** Simpan data ke file CSV harian: /data/YYYY-MM-DD.csv */
    void _saveToSD(const SensorData &data);

    /** Tambahkan data ke antrian pending: /pending.csv */
    void _addToPending(const SensorData &data);

    /** Sync semua data pending ke Firebase */
    void _syncPending();

    /** Format SensorData ke baris CSV */
    String _toCSV(const SensorData &data);

    /** Hitung jumlah baris di file pending */
    void _countPending();
};
