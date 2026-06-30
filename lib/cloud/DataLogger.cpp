#include "DataLogger.h"
#include "FS.h"
#include "SD.h"

// ═══════════════════════════════════════════════════════════════════
//  INISIALISASI
// ═══════════════════════════════════════════════════════════════════

void DataLogger::begin()
{
    // ── 1. Koneksi WiFi ──────────────────────────────────────────
    _connectWiFi();

    // ── 2. Inisialisasi SD Card ──────────────────────────────────
    if (_sd.begin())
    {
        _sdReady = true;
        Serial.println(F("[DataLogger] SD Card OK"));
        _sd.printCardInfo();

        // Buat folder /data jika belum ada
        if (!SD.exists("/data"))
            SD.mkdir("/data");

        // Hitung pending
        _countPending();
        if (_pendingCount > 0)
            Serial.printf("[DataLogger] %u data pending menunggu sync\n", _pendingCount);
    }
    else
    {
        Serial.println(F("[DataLogger] SD Card GAGAL — data offline tidak tersedia"));
    }

    // ── 3. Inisialisasi Firebase ─────────────────────────────────
    if (isWiFiConnected())
    {
        _firebase.begin(FIREBASE_API_KEY, FIREBASE_DB_URL, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD);
    }
    else
    {
        Serial.println(F("[DataLogger] WiFi offline — Firebase ditunda"));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  LOG DATA
// ═══════════════════════════════════════════════════════════════════

void DataLogger::log(const SensorData &data)
{
    // ── Selalu simpan ke SD Card (log harian) ────────────────────
    if (_sdReady)
        _saveToSD(data);

    // ── Kirim ke Firebase jika online ────────────────────────────
    if (isWiFiConnected() && _firebase.isReady())
    {
        bool sent = _firebase.sendReading(data);
        if (sent)
        {
            // Berhasil kirim → update status perangkat juga
            _firebase.updateStatus();

            // Cek apakah ada data pending untuk di-sync
            if (_pendingCount > 0)
                _syncPending();
        }
        else
        {
            // Gagal kirim (server error, timeout, dll)
            // Simpan ke pending agar bisa di-retry nanti
            if (_sdReady)
                _addToPending(data);
        }
    }
    else
    {
        // Offline → simpan ke antrian pending
        if (_sdReady)
            _addToPending(data);

        Serial.printf("[DataLogger] Offline — data disimpan ke SD (%u pending)\n",
                      _pendingCount);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  MAINTAIN (panggil di loop)
// ═══════════════════════════════════════════════════════════════════

void DataLogger::maintain()
{
    // Jika WiFi putus, coba reconnect secara periodik
    if (!isWiFiConnected())
    {
        uint32_t now = millis();
        if (now - _lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)
        {
            _lastReconnectAttempt = now;
            Serial.println(F("[DataLogger] WiFi terputus — mencoba reconnect..."));

            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

            // Tunggu sebentar (non-blocking sebagian besar waktu)
            uint32_t start = millis();
            while (WiFi.status() != WL_CONNECTED &&
                   (millis() - start) < 5000) // max 5 detik
            {
                delay(250);
            }

            if (isWiFiConnected())
            {
                Serial.printf("[DataLogger] Reconnect berhasil! IP: %s\n",
                              WiFi.localIP().toString().c_str());

                // Inisialisasi Firebase jika belum
                if (!_firebase.isReady())
                    _firebase.begin(FIREBASE_API_KEY, FIREBASE_DB_URL, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD);

                // Sync pending data
                if (_pendingCount > 0)
                    _syncPending();
            }
            else
            {
                Serial.println(F("[DataLogger] Reconnect gagal — coba lagi nanti"));
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  WIFI
// ═══════════════════════════════════════════════════════════════════

void DataLogger::_connectWiFi()
{
    Serial.printf("[WiFi] Menyambung ke \"%s\"", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < WIFI_CONNECT_TIMEOUT_MS)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WiFi] Terhubung! IP: %s | RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    else
    {
        Serial.println(F("[WiFi] GAGAL terhubung — mode offline aktif"));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  SD CARD — SIMPAN CSV HARIAN
// ═══════════════════════════════════════════════════════════════════

void DataLogger::_saveToSD(const SensorData &data)
{
    // Ekstrak tanggal dari timestamp "YYYY-MM-DD HH:MM:SS" → "YYYY-MM-DD"
    String dateStr = data.timestamp.substring(0, 10);
    String path = "/data/" + dateStr + ".csv";

    // Jika file baru, tulis header CSV terlebih dahulu
    if (!SD.exists(path.c_str()))
    {
        _sd.writeText(path.c_str(),
                      "timestamp,ph,tds_ppm,turbidity_ntu,suhu_air,suhu_udara,wqi,kategori\n");
    }

    // Append data
    String csv = _toCSV(data);
    _sd.appendText(path.c_str(), csv.c_str());
}

// ═══════════════════════════════════════════════════════════════════
//  SD CARD — ANTRIAN PENDING
// ═══════════════════════════════════════════════════════════════════

void DataLogger::_addToPending(const SensorData &data)
{
    String csv = _toCSV(data);
    _sd.appendText("/pending.csv", csv.c_str());
    _pendingCount++;
}

void DataLogger::_syncPending()
{
    if (!_sdReady || !SD.exists("/pending.csv"))
    {
        _pendingCount = 0;
        return;
    }

    Serial.printf("[DataLogger] Sync %u data pending...\n", _pendingCount);

    File file = SD.open("/pending.csv", FILE_READ);
    if (!file)
        return;

    uint16_t synced = 0;
    uint16_t failed = 0;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        // Parse CSV: timestamp,ph,tds,ntu,suhu_air,suhu_udara,wqi,kategori
        SensorData d;
        char katBuf[20] = {0};

        // Gunakan sscanf untuk parsing
        float ph, tds, ntu, sa, su, wqi;
        char tsBuf[24] = {0};

        int parsed = sscanf(line.c_str(),
                            "%23[^,],%f,%f,%f,%f,%f,%f,%19s",
                            tsBuf, &ph, &tds, &ntu, &sa, &su, &wqi, katBuf);

        if (parsed >= 7)
        {
            d.timestamp = String(tsBuf);
            d.ph = ph;
            d.tds_ppm = tds;
            d.turbidity_ntu = ntu;
            d.suhu_air = sa;
            d.suhu_udara = su;
            d.wqi = wqi;
            d.kategori = (strcmp(katBuf, "Layak") == 0)   ? "Layak"
                         : (strcmp(katBuf, "Perlu") == 0) ? "Perlu Perlakuan"
                                                          : "Tidak Layak";

            if (_firebase.sendReading(d, true))
            {
                synced++;
            }
            else
            {
                failed++;
                break; // Jika gagal, hentikan sync — coba lagi nanti
            }
        }

        // Yield agar watchdog tidak timeout
        yield();
    }

    file.close();

    if (failed == 0)
    {
        // Semua berhasil di-sync → hapus file pending
        SD.remove("/pending.csv");
        _pendingCount = 0;
        Serial.printf("[DataLogger] Sync selesai! %u data berhasil dikirim\n", synced);
    }
    else
    {
        // Ada yang gagal — buat file pending baru dari yang belum terkirim
        // Untuk simplicity, kita hitung ulang sisa pending
        _pendingCount = _pendingCount - synced;
        Serial.printf("[DataLogger] Sync parsial: %u terkirim, %u tersisa\n",
                      synced, _pendingCount);

        // Tulis ulang file pending tanpa baris yang sudah terkirim
        // Baca semua, skip baris yang sudah terkirim, tulis ulang
        File src = SD.open("/pending.csv", FILE_READ);
        if (src)
        {
            String remaining = "";
            uint16_t skip = synced;
            while (src.available())
            {
                String ln = src.readStringUntil('\n');
                ln.trim();
                if (ln.length() == 0)
                    continue;
                if (skip > 0)
                {
                    skip--;
                    continue;
                }
                remaining += ln + "\n";
            }
            src.close();

            // Overwrite pending.csv
            _sd.writeText("/pending.csv", remaining.c_str());
        }
    }
}

void DataLogger::_countPending()
{
    if (!_sdReady || !SD.exists("/pending.csv"))
    {
        _pendingCount = 0;
        return;
    }

    File file = SD.open("/pending.csv", FILE_READ);
    if (!file)
    {
        _pendingCount = 0;
        return;
    }

    _pendingCount = 0;
    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0)
            _pendingCount++;
    }
    file.close();
}

// ═══════════════════════════════════════════════════════════════════
//  UTILITAS
// ═══════════════════════════════════════════════════════════════════

String DataLogger::_toCSV(const SensorData &data)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "%s,%.2f,%.0f,%.1f,%.1f,%.2f,%.1f,%s",
             data.timestamp.c_str(),
             data.ph,
             data.tds_ppm,
             data.turbidity_ntu,
             data.suhu_air,
             data.suhu_udara,
             data.wqi,
             data.kategori);
    return String(buf);
}
