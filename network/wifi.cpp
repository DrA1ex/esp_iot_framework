#include "wifi.h"

#ifdef ESP32

#include <WiFi.h>

#define __WIFI_STA wifi_mode_t::WIFI_STA
#define __WIFI_STA_AP wifi_mode_t::WIFI_AP_STA
#define __WIFI_AP wifi_mode_t::WIFI_AP
#define __ID (ESP.getEfuseMac() & 0xffff)
#elif ESP8266

#include <ESP8266WiFi.h>

#define __WIFI_STA WiFiMode::WIFI_STA
#define __WIFI_STA_AP WiFiMode::WIFI_AP_STA
#define __WIFI_AP WiFiMode::WIFI_AP
#define __ID (ESP.getChipId() & 0xffff)
#else
#pragma message "Platform not supported"

#define __WIFI_STA 0
#define __WIFI_STA_AP 0
#define __WIFI_AP 0
#define __ID 0
#endif

#include "lib/debug.h"

WifiManager::WifiManager(const char *ssid, const char *password, unsigned long connection_check_interval) :
    _ssid(ssid), _password(password), _connection_check_interval(connection_check_interval) {
    if (strlen(_password) < 8) _password = "12345678";
}

void WifiManager::connect(WifiMode mode, unsigned long connection_interval) {
    if (_state == WifiManagerState::CONNECTING) return;

    WiFi.disconnect();

    _mode = mode;
    _connection_interval = connection_interval;
    _state = WifiManagerState::DISCONNECTED;

    switch (_mode) {
        case WifiMode::AP:
            _connect_ap();
            break;

        case WifiMode::STA:
        case WifiMode::STA_AP:
            _connect_sta_step();
            break;

        default:;
    }
}

void WifiManager::handle_connection() {
    if (_state == WifiManagerState::CONNECTING) {
        if (_mode != WifiMode::AP) _connect_sta_step();

        return;
    }

    if (millis() - _last_connection_check < _connection_check_interval) return;

    _last_connection_check = millis();
    if (WiFi.getMode() != __WIFI_AP && !WiFi.isConnected()) {
        D_PRINT("Wi-Fi connection lost");

        _state = WifiManagerState::DISCONNECTED;
        connect(_mode);
    }
}

void WifiManager::_connect_ap() {
    if (_state != WifiManagerState::DISCONNECTED) return;

    WiFi.mode(__WIFI_AP);

    String chip_id = String(__ID, HEX);
    chip_id.toUpperCase();

    static String ssid = String(_ssid) + "_" + chip_id;
    WiFi.softAP(ssid, _password);

    D_PRINTF("Wi-Fi connected! Mode: AP, SSID: %s, Password: %s, IP address: ", ssid.c_str(), _password);
    D_WRITE(WiFi.softAPIP());
    D_PRINTF(", Channel: %d\r\n", WiFi.channel());

    _ssid = ssid.c_str();
    _state = WifiManagerState::CONNECTED;
}

void WifiManager::_connect_sta_step() {
    if (_state == WifiManagerState::CONNECTED) return;

    if (_state == WifiManagerState::DISCONNECTED) {
        D_PRINT("Connecting to Wi-Fi...");

        if (_mode == WifiMode::STA) WiFi.mode(__WIFI_STA);
        else WiFi.mode(__WIFI_STA_AP);

        WiFi.begin(_ssid, _password);

        _state = WifiManagerState::CONNECTING;
        _connection_begin_time = millis();
        return;
    }

    if (WiFi.isConnected()) {
        D_PRINTF("Wi-Fi connected! Mode: STA, SSID: %s, IP address: ", _ssid);
        D_WRITE(WiFi.localIP());
        D_PRINTF(", Channel: %d\r\n", WiFi.channel());

        _state = WifiManagerState::CONNECTED;
        return;
    }

    if (_connection_interval > 0 && millis() - _connection_begin_time > _connection_interval) {
        D_PRINT("Wi-Fi connection interval exceeded. Switch to AP mode.");

        _state = WifiManagerState::DISCONNECTED;
        connect(WifiMode::AP);
    }
}
