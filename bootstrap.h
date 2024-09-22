#pragma once

#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#include "base/metadata.h"
#include "network/web.h"
#include "network/wifi.h"
#include "network/server/mqtt.h"
#include "network/server/ws.h"
#include "misc/event_topic.h"
#include "misc/storage.h"
#include "utils/enum.h"
#include "utils/qr.h"

#ifndef RESTART_DELAY
#define RESTART_DELAY                           (500u)
#endif

#ifndef BOOTSTRAP_SERVICE_LOOP_INTERVAL
#define BOOTSTRAP_SERVICE_LOOP_INTERVAL         (20u)
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

MAKE_ENUM_AUTO(BootstrapState, uint8_t,
               UNINITIALIZED,
               WIFI_CONNECT,
               INITIALIZING,
               READY,
)

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

    BootstrapConfig _bootstrap_config{};

    EventTopic<BootstrapState> _event_state_changed;

public:
    explicit Bootstrap(FS *fs);

    inline ConfigT &config() { return _config_storage.get(); }

    inline EventTopic<BootstrapState> &event_state_changed() { return _event_state_changed; }
    inline Timer &timer() { return _timer; }
    inline auto &wifi_manager() { return _wifi_manager; }

    inline auto &web_server() { return _web_server; }
    inline auto &ws_server() { return _ws_server; }
    inline auto &mqtt_server() { return _mqtt_server; }

    void begin(BootstrapConfig bootstrap_config);
    void event_loop();

    void save_changes();
    void restart();

private:
    void _change_state(BootstrapState state);
    void _after_init();
    void _service_loop();
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

    _timer.add_interval([this](auto) { this->_service_loop(); }, BOOTSTRAP_SERVICE_LOOP_INTERVAL);
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::save_changes() {
    _config_storage.save();
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::event_loop() {
    _timer.handle_timers();
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
void Bootstrap<ConfigT, PacketEnumT>::_change_state(BootstrapState state) {
    if (_state == state) return;

    D_PRINTF("Bootstrap: state changed to %s\r\n", __debug_enum_str(state));

    _state = state;
    _event_state_changed.publish(this, state);
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::restart() {
    D_PRINTF("Received restart signal. Restarting after %u ms.\r\n", RESTART_DELAY);

    if (_config_storage.is_pending_commit()) _config_storage.force_save();

    _timer.add_timeout([](auto) { ESP.restart(); }, RESTART_DELAY);
}

template<typename ConfigT, typename PacketEnumT>
void Bootstrap<ConfigT, PacketEnumT>::_service_loop() {
    switch (_state) {
        case BootstrapState::UNINITIALIZED:
            _wifi_manager->connect(_bootstrap_config.wifi_mode, _bootstrap_config.wifi_connection_timeout);
            _change_state(BootstrapState::WIFI_CONNECT);

            break;

        case BootstrapState::WIFI_CONNECT:
            _wifi_manager->handle_connection();
            if (_wifi_manager->state() == WifiManagerState::CONNECTED) {
                _change_state(BootstrapState::INITIALIZING);
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

            _change_state(BootstrapState::READY);
            break;

        case BootstrapState::READY:
            _wifi_manager->handle_connection();
            ArduinoOTA.handle();

            if (_dns_server) _dns_server->processNextRequest();

            _ws_server->handle_connection();
            _mqtt_server->handle_connection();
            break;
    }
}