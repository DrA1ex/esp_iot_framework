#pragma once

#include "Arduino.h"

enum class WifiManagerState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

enum class WifiMode : uint8_t {
    AP = 0,
    STA = 1
};

class WifiManager {
    const char *_ssid;
    const char *_password;
    unsigned long _connection_check_interval = 0;

    WifiMode _mode = WifiMode::AP;
    WifiManagerState _state = WifiManagerState::DISCONNECTED;

    unsigned long _connection_interval = 0;
    unsigned long _connection_begin_time = 0;
    unsigned long _last_connection_check = 0;

public:
    WifiManager(const char *ssid, const char *password, unsigned long connection_check_interval = 5000u);

    void connect(WifiMode mode, unsigned long connection_interval = 120000u);
    void handle_connection();

    inline WifiManagerState state() { return _state; }
    inline WifiMode mode() { return _mode; }

    inline const char *ssid() { return _ssid; }
    inline const char *password() { return _password; }

private:
    void _connect_ap();
    void _connect_sta_step();
};