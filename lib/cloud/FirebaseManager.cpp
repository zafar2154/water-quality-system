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

    _initialized = true;

    _config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&_config, &_auth);
    Firebase.reconnectNetwork(true);

    _fbdo.setBSSLBufferSize(2048, 1024);
    _fbdo.setResponseSize(1024);
}

bool FirebaseManager::sendReading(const SensorData &data, bool isSync)
{
    if (!_initialized || !Firebase.ready())
        return false;

    FirebaseJson json;
    _buildJson(data, json);

    if (isSync)
    {
        // ── Mode Sync (data offline) → langsung push ke sensor_history ──
        if (Firebase.RTDB.pushJSON(&_fbdo, "/sensor_history", &json))
        {
            Serial.printf("[Firebase] Sync offline → sensor_history (push key: %s)\n",
                          _fbdo.pushName().c_str());
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
        // ── Mode Live → overwrite /live_status (sama seperti live_ref.set()) ──
        if (!Firebase.RTDB.setJSON(&_fbdo, "/live_status", &json))
        {
            Serial.printf("[Firebase] GAGAL update live_status: %s\n", _fbdo.errorReason().c_str());
            return false;
        }

        _liveCount++;
        Serial.printf("[Firebase] live_status diupdate (Count: %u/10)\n", _liveCount);

        // ── Setiap 10 kali live update → push ke sensor_history ──
        if (_liveCount >= 10)
        {
            if (Firebase.RTDB.pushJSON(&_fbdo, "/sensor_history", &json))
            {
                Serial.printf("[Firebase] >>> DATA DISIMPAN KE HISTORY! (push key: %s) <<<\n",
                              _fbdo.pushName().c_str());
                _liveCount = 0; // Reset counter
            }
            else
            {
                Serial.printf("[Firebase] GAGAL push sensor_history: %s\n", _fbdo.errorReason().c_str());
                return false;
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

    Firebase.RTDB.setJSON(&_fbdo, "/device_status", &json);
}

bool FirebaseManager::isReady() const
{
    return _initialized && Firebase.ready();
}

void FirebaseManager::_buildJson(const SensorData &data, FirebaseJson &json)
{
    json.clear();
    json.set("timestamp", (int)data.timestamp);
    json.set("ph", double(data.ph));
    json.set("tds", double(data.tds));
    json.set("turbidity", double(data.turbidity));
    json.set("temperature", double(data.temperature));
    json.set("lat", data.lat);
    json.set("lon", data.lon);
}
