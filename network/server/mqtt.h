#include <AsyncMqttClient.h>

#include "../../base/application.h"
#include "../../debug.h"

#ifndef MQTT_CONNECTION_TIMEOUT
#define MQTT_CONNECTION_TIMEOUT                 (15000u)                // Connection attempt timeout to MQTT server
#endif

#ifndef MQTT_RECONNECT_TIMEOUT
#define MQTT_RECONNECT_TIMEOUT                  (5000u)                 // Time before new reconnection attempt to MQTT server
#endif

enum class MqttServerState : uint8_t {
    UNINITIALIZED,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

template<typename ApplicationT, typename = std::enable_if_t<std::is_base_of_v<
        AbstractApplication<typename ApplicationT::ConfigT, typename ApplicationT::MetaPropT>, ApplicationT>>>
class MqttServer {
public:
    using PropEnumT = typename ApplicationT::PropEnumT;
    using PacketEnumT = typename ApplicationT::PacketEnumT;
    using MetaPropT = typename ApplicationT::MetaPropT;

private:
    AsyncMqttClient _mqttClient;

    ApplicationT &_app;

    MqttServerState _state = MqttServerState::UNINITIALIZED;
    unsigned long _state_change_time = 0;
    unsigned long _last_connection_attempt_time = 0;

public:
    explicit MqttServer(ApplicationT &app);

    void begin(const char *host, uint16_t port, const char *user, const char *password);

    void handle_connection();

protected:
    inline ApplicationT &app() { return _app; }

    void _on_connect(bool sessionPresent);
    void _on_disconnect(AsyncMqttClientDisconnectReason reason);
    void _on_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

    void _subscribe(const char *topic, uint8_t qos);
    void _publish(const char *topic, uint8_t qos, const char *payload, size_t length);

    void _process_message(const String &topic, const String &payload);
    void _process_notification(PropEnumT prop);

    void _change_state(MqttServerState state);
    void _connect();

    template<typename T>
    void _set_value(const MetaPropT &meta, const T &value);

    template<typename T>
    void _notify_value_changed(const MetaPropT &meta);


    virtual void _transform_topic_payload(const String &topic, String &payload) {};
    virtual void _transform_topic_notification(const MetaPropT &meta, String &payload) {};

    virtual void _after_message_process(const MetaPropT &meta);
};

template<typename ApplicationT, typename C1>
MqttServer<ApplicationT, C1>::MqttServer(ApplicationT &app) : _app(app) {}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::begin(const char *host, uint16_t port, const char *user, const char *password) {
    if (_state != MqttServerState::UNINITIALIZED) return;

    using namespace std::placeholders;

    _mqttClient.onConnect(std::bind(&MqttServer::_on_connect, this, _1));
    _mqttClient.onDisconnect(std::bind(&MqttServer::_on_disconnect, this, _1));
    _mqttClient.onMessage(std::bind(&MqttServer::_on_message, this, _1, _2, _3, _4, _5, _6));

    _mqttClient.setServer(host, port);
    _mqttClient.setCredentials(user, password);

    _app.event_property_changed().subscribe(this, [this](auto, auto prop, auto) {
        _process_notification(prop);
    });

    _connect();
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_connect() {
    _change_state(MqttServerState::CONNECTING);

    _last_connection_attempt_time = millis();
    _mqttClient.connect();
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_change_state(MqttServerState state) {
    _state = state;
    _state_change_time = millis();
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::handle_connection() {
    if (_state == MqttServerState::UNINITIALIZED) return;

    if (_state == MqttServerState::DISCONNECTED && (millis() - _last_connection_attempt_time) > MQTT_RECONNECT_TIMEOUT) {
        D_PRINT("MQTT Reconnecting...");

        _connect();
    }

    if (_state == MqttServerState::CONNECTING && !_mqttClient.connected() && (millis() - _state_change_time) > MQTT_CONNECTION_TIMEOUT) {
        D_PRINT("MQTT Connection timeout");

        _change_state(MqttServerState::DISCONNECTED);
        _mqttClient.disconnect(true);
    }
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_on_connect(bool) {
    D_PRINT("MQTT Connected");

    for (const auto &[topic, _]: _app.topic_property_meta()) {
        _subscribe(topic.c_str(), 1);
    }

    _last_connection_attempt_time = millis();
    _change_state(MqttServerState::CONNECTED);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_on_disconnect(AsyncMqttClientDisconnectReason reason) {
    D_PRINTF("MQTT Disconnected. Reason %u\n", (uint8_t) reason);

    _change_state(MqttServerState::DISCONNECTED);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_on_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                                               size_t len, size_t index, size_t total) {
    D_PRINTF("MQTT Received: %s: \"%.*s\"\n", topic, len, payload);

    String topic_str(topic);
    String payload_str{};
    payload_str.concat(payload, len);

    _transform_topic_payload(topic_str, payload_str);

    _process_message(topic_str, payload_str);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_subscribe(const char *topic, uint8_t qos) {
    _mqttClient.subscribe(topic, qos);
    D_PRINTF("MQTT Subscribe: \"%s\"\n", topic);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_publish(const char *topic, uint8_t qos, const char *payload, size_t length) {
    if (_state != MqttServerState::CONNECTED) {
        D_PRINTF("MQTT Not connected. Skip message to %s\n", topic);
        return;
    };

    _mqttClient.publish(topic, qos, true, payload, length);

    D_PRINTF("MQTT Publish: %s: \"%.*s\"\n", topic, length, payload);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_process_message(const String &topic, const String &payload) {
    auto iter_meta = _app.topic_property_meta().find(topic);
    if (iter_meta == _app.topic_property_meta().end()) {
        D_PRINTF("MQTT: Message in unsupported topic: %s\n", topic.c_str());
        return;
    }

    const auto meta = iter_meta->second;
    switch (meta.value_size) {
        case 8:
            _set_value(meta, (uint64_t) payload.toInt());
            break;

        case 4:
            _set_value(meta, (uint32_t) payload.toInt());
            break;

        case 2:
            _set_value(meta, (uint16_t) payload.toInt());
            break;

        case 1:
            _set_value(meta, (uint8_t) payload.toInt());
            break;

        default:
            D_PRINTF("MQTT: Unsupported value size: %u. topic: %s\n", meta.value_size, topic.c_str());
            return;
    }

    _after_message_process(meta);
}

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_after_message_process(const MetaPropT &meta) {
    _app.event_property_changed().publish(this, meta.property);
};

template<typename ApplicationT, typename C1>
void MqttServer<ApplicationT, C1>::_process_notification(PropEnumT prop) {
    auto iter_meta = _app.property_meta().find(prop);
    if (iter_meta == _app.property_meta().end()) return;

    const auto meta = iter_meta->second[0];
    if (meta.mqtt_out_topic == nullptr) return;

    D_PRINTF("MQTT: Processing notification for %s\n", __debug_enum_str(prop));

    switch (meta.value_size) {
        case 8:
            _notify_value_changed<uint64_t>(meta);
            break;

        case 4:
            _notify_value_changed<uint32_t>(meta);
            break;

        case 2:
            _notify_value_changed<uint16_t>(meta);
            break;

        case 1:
            _notify_value_changed<uint8_t>(meta);
            break;

        default:
            D_PRINTF("MQTT: Unsupported value size: %u\n", meta.value_size);
            return;
    }
}

template<typename ApplicationT, typename C1>
template<typename T>
void MqttServer<ApplicationT, C1>::_notify_value_changed(const MetaPropT &meta) {
    T value;
    memcpy(&value, (uint8_t *) &_app.config() + meta.value_offset, sizeof(value));

    auto value_str = String(value);
    _transform_topic_notification(meta, value_str);

    _publish(meta.mqtt_out_topic, 1, value_str.c_str(), value_str.length());
}

template<typename ApplicationT, typename C1>
template<typename T>
void MqttServer<ApplicationT, C1>::_set_value(const MetaPropT &meta, const T &value) {
    memcpy((uint8_t *) &_app.config() + meta.value_offset, &value, sizeof(value));

    D_PRINTF("Set %s = ", __debug_enum_str(meta.property));
    D_PRINT_HEX(((uint8_t *) (&value)), sizeof(value));
}
