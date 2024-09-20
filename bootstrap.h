#pragma once

#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#include "base/metadata.h"
#include "misc/storage.h"
#include "network/web.h"
#include "network/wifi.h"
#include "network/server/mqtt.h"
#include "network/server/ws.h"
#include "utils/qr.h"

#ifndef RESTART_DELAY
#define RESTART_DELAY                           (500u)
#endif


struct BootstrapConfig {
    const char *mdns_name;

    WifiMode wifi_mode;
    const char *wifi_ssid;
    const char *wifi_password;

    uint32_t wifi_connection_timeout;

    bool mqtt_enabled;
    const char *mqtt_host;
    uint16_t mqtt_port;
    const char *mqtt_user;
    const char *mqtt_password;
};

enum class BootstrapState {
    UNINITIALIZED,
    WIFI_CONNECT,
    INITIALIZING,
    READY,
};

template<typename ConfigT, typename PacketEnumT>
class Bootstrap {
    std::unique_ptr<WifiManager> _wifi_manager = nullptr;

    std::unique_ptr<WebSocketServer<PacketEnumT>> _ws_server = nullptr;
    std::unique_ptr<MqttServer> _mqtt_server = nullptr;

    FS *_fs;

    Timer _timer;
    Storage<ConfigT> _config_storage;
    WebServer _web_server{};

    BootstrapState _state = BootstrapState::UNINITIALIZED;
    std::unique_ptr<DNSServer> _dns_server = nullptr;

    BootstrapConfig _bootstrap_config;

public:
    explicit Bootstrap(FS *fs);

    inline ConfigT &config() { return _config_storage.get(); }

    inline Timer &timer() { return _timer; }
    inline auto &wifi_manager() { return _wifi_manager; }

    inline auto &web_server() { return _web_server; }
    inline auto &ws_server() { return _ws_server; }
    inline auto &mqtt_server() { return _mqtt_server; }

    void begin(BootstrapConfig bootstrap_config);
    BootstrapState event_loop();

    void save_changes();
    void restart();

private:
    void _after_init();
};


template<typename ConfigT, typename PacketEnumT>
Bootstrap<ConfigT, PacketEnumT>::Bootstrap(FS *fs) : _fs(fs), _config_storage(_timer, "config") {
    _config_storage.begin(_fs);
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::begin(BootstrapConfig bootstrap_config) {
    _bootstrap_config = bootstrap_config;

    _wifi_manager = std::make_unique<WifiManager>(_bootstrap_config.wifi_ssid, _bootstrap_config.wifi_password);
    _ws_server = std::make_unique<WebSocketServer<PacketEnumT>>();
    _mqtt_server = std::make_unique<MqttServer>();
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::save_changes() {
    _config_storage.save();
}

template<typename ConfigT, typename PacketEnumT>
BootstrapState Bootstrap<ConfigT, PacketEnumT>::event_loop() {
    switch (_state) {
        case BootstrapState::UNINITIALIZED:
            _wifi_manager->connect(_bootstrap_config.wifi_mode, _bootstrap_config.wifi_connection_timeout);
            _state = BootstrapState::WIFI_CONNECT;

            break;

        case BootstrapState::WIFI_CONNECT:
            _wifi_manager->handle_connection();
            if (_wifi_manager->state() == WifiManagerState::CONNECTED) {
                _state = BootstrapState::INITIALIZING;
            }

            break;

        case BootstrapState::INITIALIZING:
            ArduinoOTA.setHostname(_bootstrap_config.mdns_name);
            ArduinoOTA.begin();

            _ws_server->begin(_web_server);
            _web_server.begin(_fs);

            if (_bootstrap_config.mqtt_enabled) {
                _mqtt_server->set_prefix(_bootstrap_config.mdns_name);
                _mqtt_server->begin(_bootstrap_config.mqtt_host, _bootstrap_config.mqtt_port,
                                    _bootstrap_config.mqtt_user, _bootstrap_config.mqtt_password);
            }

            D_PRINT("ESP Ready");
            _after_init();

            _state = BootstrapState::READY;
            break;

        case BootstrapState::READY:
            _wifi_manager->handle_connection();
            ArduinoOTA.handle();

            if (_dns_server) _dns_server->processNextRequest();

            _timer.handle_timers();

            _ws_server->handle_connection();
            _mqtt_server->handle_connection();
            break;
    }

    return _state;
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::_after_init() {
    if (_wifi_manager->mode() == WifiMode::AP) {
        _dns_server = std::make_unique<DNSServer>();
        _dns_server->start(53, "*", WiFi.softAPIP());

        D_PRINT("Connect to WiFi:");
        qr_print_wifi_connection(_wifi_manager->ssid(), _wifi_manager->password());
    } else {
        String url = "http://";
        url += _bootstrap_config.mdns_name;
        url += ".local";

        if (_web_server.port() != 80) {
            url += ":";
            url += _web_server.port();
        }

        D_PRINT("Open WebUI:");
        qr_print_string(url.c_str());
    }
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::restart() {
    D_PRINTF("Received restart signal. Restarting after %u ms.\r\n", RESTART_DELAY);

    if (_config_storage.is_pending_commit()) _config_storage.force_save();

    _timer.add_timeout([](auto) { ESP.restart(); }, RESTART_DELAY);
}
