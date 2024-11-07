#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "async_now.h"

struct __attribute__((__packed__)) EspNowInteractionPacketHeader {
    uint8_t id;
    bool is_response;
    uint8_t index;
    uint8_t count;
    uint8_t size;
};

struct EspNowSendResponse {
    uint8_t id;
};

struct EspNowMessage {
    uint8_t id;
    uint8_t mac_addr[6];
    uint8_t received_count;
    uint8_t parts_count;
    uint16_t size;
    std::shared_ptr<uint8_t[]> data;
};

union EspNowMessageKey {
    struct __attribute__((__packed__)) {
        uint8_t id;
        bool is_response;
        uint8_t mac_addr[6];
    } fields;

    uint64_t u64;
    uint8_t bytes[8];
};

constexpr uint8_t ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH = sizeof(EspNowInteractionPacketHeader);
constexpr uint8_t ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH = ESP_NOW_MAX_DATA_LEN - ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH;
constexpr uint16_t ESP_NOW_INTERACTION_MAX_DATA_LENGTH = 0xff * ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH;

class AsyncEspNowInteraction {
    static AsyncEspNowInteraction _instance;

    bool _initialized = false;
    AsyncEspNow &_async_now = AsyncEspNow::instance();
    uint8_t _id = 0;

    std::unordered_map<uint8_t, std::shared_ptr<Promise<EspNowMessage>>> _requests;
    std::unordered_map<uint64_t, EspNowMessage> _messages;

    std::function<void(EspNowMessage)> _on_message_cb;

    AsyncEspNowInteraction() = default;

public:
    static const uint8_t BROADCAST_MAC[6];
    static AsyncEspNowInteraction &instance() { return _instance; }

    AsyncEspNowInteraction(const AsyncEspNowInteraction &) = delete;
    AsyncEspNowInteraction &operator=(AsyncEspNowInteraction const &) = delete;

    bool begin();
    void end();

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    Future<EspNowSendResponse> send(const uint8_t *mac_addr, const T &value);
    Future<EspNowSendResponse> send(const uint8_t *mac_addr, const char *str);
    Future<EspNowSendResponse> send(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    Future<EspNowMessage> request(const uint8_t *mac_addr, const T &value);
    Future<EspNowMessage> request(const uint8_t *mac_addr, const char *str);
    Future<EspNowMessage> request(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, const T &value);
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, const char *str);
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    void set_on_message_cb(std::function<void(EspNowMessage)> cb) { _on_message_cb = std::move(cb); }

    Future<uint8_t> discover_peer_channel(const uint8_t *mac_addr);

    static void print_mac() { D_PRINTF("Mac: %s\r\n", WiFi.macAddress().c_str()); }

private:
    Future<EspNowSendResponse> _send_impl(uint8_t id, bool is_response, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);
    Future<EspNowMessage> _request_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    Future<uint8_t> _configure_peer_channel(const uint8_t *mac_addr, uint8_t channel);

    void _on_packet_received(EspNowPacket packet);
};

template<typename T, typename>
Future<EspNowSendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const T &value) {
    return send(mac_addr, &value, sizeof(T));
}

template<typename T, typename>
Future<EspNowMessage> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const T &value) {
    return request(mac_addr, &value, sizeof(T));
}
template<typename T, typename>
Future<void> AsyncEspNowInteraction::respond(uint8_t id, const uint8_t *mac_addr, const T &value) {
    return respond(id, mac_addr, &value, sizeof(T));
}

#endif
