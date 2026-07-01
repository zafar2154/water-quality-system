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
            // Gagal kirim → simpan ke pending
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
    if (!isWiFiConnected())
    {
        uint32_t now = millis();
        if (now - _lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)
        {
            _lastReconnectAttempt = now;
            Serial.println(F("[DataLogger] WiFi terputus — mencoba reconnect..."));

            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

            uint32_t start = millis();
            while (WiFi.status() != WL_CONNECTED &&
                   (millis() - start) < 5000)
            {
                delay(250);
            }

            if (isWiFiConnected())
            {
                Serial.printf("[DataLogger] Reconnect berhasil! IP: %s\n",
                              WiFi.localIP().toString().c_str());

                if (!_firebase.isReady())
                    _firebase.begin(FIREBASE_API_KEY, FIREBASE_DB_URL, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD);

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
    // Buat nama file berdasarkan hari (dari epoch timestamp)
    // Gunakan epoch / 86400 sebagai penanda hari
    char dateBuf[16];
    unsigned long dayNum = data.timestamp / 86400;
    snprintf(dateBuf, sizeof(dateBuf), "%lu", dayNum);
    String path = "/data/day_" + String(dateBuf) + ".csv";

    // Jika file baru, tulis header CSV terlebih dahulu
    if (!SD.exists(path.c_str()))
    {
        _sd.writeText(path.c_str(),
                      "timestamp,ph,tds,turbidity,temperature,lat,lon\n");
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

        // Parse CSV: timestamp,ph,tds,turbidity,temperature,lat,lon
        SensorData d;
        unsigned long ts;
        float ph, tds, turb, temp;
        double lat, lon;

        int parsed = sscanf(line.c_str(),
                            "%lu,%f,%f,%f,%f,%lf,%lf",
                            &ts, &ph, &tds, &turb, &temp, &lat, &lon);

        if (parsed >= 5) // lat/lon mungkin tidak ada (0)
        {
            d.timestamp = ts;
            d.ph = ph;
            d.tds = tds;
            d.turbidity = turb;
            d.temperature = temp;
            d.lat = (parsed >= 6) ? lat : 0.0;
            d.lon = (parsed >= 7) ? lon : 0.0;

            if (_firebase.sendReading(d, true)) // isSync = true → push ke history
            {
                synced++;
            }
            else
            {
                failed++;
                break;
            }
        }

        yield();
    }

    file.close();

    if (failed == 0)
    {
        SD.remove("/pending.csv");
        _pendingCount = 0;
        Serial.printf("[DataLogger] Sync selesai! %u data berhasil dikirim\n", synced);
    }
    else
    {
        _pendingCount = _pendingCount - synced;
        Serial.printf("[DataLogger] Sync parsial: %u terkirim, %u tersisa\n",
                      synced, _pendingCount);

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
             "%lu,%.2f,%.2f,%.2f,%.1f,%.6f,%.6f",
             data.timestamp,
             data.ph,
             data.tds,
             data.turbidity,
             data.temperature,
             data.lat,
             data.lon);
    return String(buf);
}
