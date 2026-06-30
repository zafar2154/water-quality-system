#include "FirebaseManager.h"
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

void FirebaseManager::begin(const char *apiKey, const char *dbUrl, const char *email, const char *password)
{
    _config.api_key = apiKey;
    _config.database_url = dbUrl;

    // Autentikasi dengan Email dan Password
    _auth.user.email = email;
    _auth.user.password = password;

    Serial.printf("[Firebase] Mencoba login dengan email: %s\n", email);

    // Tetap tandai terinisialisasi agar loop pengiriman data tetap berjalan
    // (library akan terus mencoba autentikasi di background jika gagal)
    _initialized = true;

    // Assign callback untuk token status (opsional tapi berguna untuk debug)
    _config.token_status_callback = tokenStatusCallback; // dari TokenHelper.h

    Firebase.begin(&_config, &_auth);
    Firebase.reconnectNetwork(true);

    // Set ukuran buffer yang wajar untuk ESP32
    _fbdo.setBSSLBufferSize(2048, 1024);
    _fbdo.setResponseSize(1024);
}

bool FirebaseManager::sendReading(const SensorData &data, bool isSync)
{
    if (!_initialized || !Firebase.ready())
        return false;

    FirebaseJson json;
    json.set("timestamp", data.timestamp);
    json.set("ph", double(data.ph));
    json.set("tds_ppm", double(data.tds_ppm));
    json.set("turbidity_ntu", double(data.turbidity_ntu));
    json.set("suhu_air", double(data.suhu_air));
    json.set("suhu_udara", double(data.suhu_udara));
    json.set("wqi", double(data.wqi));
    json.set("kategori", data.kategori);

    if (isSync)
    {
        // Mode Sync (Offline Data) -> langsung masuk ke sensor_history
        String key = _makeKey(data.timestamp);
        String historyPath = "/sensor_history/" + key;
        if (Firebase.RTDB.setJSON(&_fbdo, historyPath.c_str(), &json))
        {
            Serial.printf("[Firebase] Sync offline -> %s\n", historyPath.c_str());
            return true;
        }
        else
        {
            Serial.printf("[Firebase] GAGAL sync offline: %s\n", _fbdo.errorReason().c_str());
            return false;
        }
    }
    else
    {
        // Mode Live -> Update live_status (overwrite)
        if (!Firebase.RTDB.setJSON(&_fbdo, "/live_status", &json))
        {
            Serial.printf("[Firebase] GAGAL update live_status: %s\n", _fbdo.errorReason().c_str());
            return false;
        }

        _liveCount++;
        Serial.printf("[Firebase] live_status diupdate (Count: %u/10)\n", _liveCount);

        // Jika sudah 10 kali, duplikat ke sensor_history
        if (_liveCount >= 10)
        {
            String key = _makeKey(data.timestamp);
            String historyPath = "/sensor_history/" + key;
            if (Firebase.RTDB.setJSON(&_fbdo, historyPath.c_str(), &json))
            {
                Serial.printf("[Firebase] Data masuk ke sensor_history -> %s\n", historyPath.c_str());
                _liveCount = 0; // Reset
            }
            else
            {
                Serial.printf("[Firebase] GAGAL update sensor_history: %s\n", _fbdo.errorReason().c_str());
                return false; // Return false agar masuk ke antrian offline pending.csv
            }
        }
        return true;
    }
}

void FirebaseManager::updateStatus()
{
    if (!_initialized || !Firebase.ready())
        return;

    FirebaseJson json;
    json.set("wifi_rssi", WiFi.RSSI());
    json.set("uptime_sec", (int)(millis() / 1000));
    json.set("free_heap", (int)ESP.getFreeHeap());

    Firebase.RTDB.setJSON(&_fbdo, "/status", &json);
}

bool FirebaseManager::isReady() const
{
    return _initialized && Firebase.ready();
}

String FirebaseManager::_makeKey(const String &timestamp)
{
    // "2026-06-30 14:51:07" → "2026-06-30_14-51-07"
    String key = timestamp;
    key.replace(' ', '_');
    key.replace(':', '-');
    return key;
}
