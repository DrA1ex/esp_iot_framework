#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include "../base/async_now_interactions.h"

enum class SpecialPacketTypes: uint8_t {
    PING      = 0xf0,
    DISCOVERY = 0xf1,

    SYSTEM_RESPONSE = 0xff
};

struct __attribute__((__packed__)) NowPacketHeader {
    uint8_t type;
    uint8_t count;
};

struct NowPacket {
    uint8_t id;
    uint8_t mac_addr[6];
    uint8_t type;
    uint8_t count;
    uint8_t *data;
    uint16_t size;

    static NowPacket parse(const EspNowMessage &message);

private:
    std::shared_ptr<uint8_t[]> _message_data;
};

typedef std::function<void(NowPacket)> NowIoPaketCb;

class NowIo {
    static NowIo _instance;

    AsyncEspNowInteraction &_interaction = AsyncEspNowInteraction::instance();

    NowIoPaketCb _on_packet_cb;

public:
    static NowIo &instance() { return _instance; }

    NowIo(const NowIo &) = delete;
    NowIo &operator=(NowIo const &) = delete;


    bool begin();
    void end();

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items);
    template<typename T, size_t Count, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]);
    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send(const uint8_t *mac_addr, uint8_t type, const T &item);
    Future<void> send(const uint8_t *mac_addr, uint8_t type);
    Future<void> send(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<NowPacket> request(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items);
    template<typename T, size_t Count, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<NowPacket> request(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]);
    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<NowPacket> request(const uint8_t *mac_addr, uint8_t type, const T &item);
    Future<NowPacket> request(const uint8_t *mac_addr, uint8_t type);
    Future<NowPacket> request(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items);
    template<typename T, size_t Count, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]);
    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const T &item);
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, uint8_t type);
    Future<void> respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    Future<void> ping(const uint8_t *mac_addr);
    Future<void> discovery(uint8_t *out_mac_addr);

    void set_on_packet_cb(NowIoPaketCb on_packet_cb) { _on_packet_cb = std::move(on_packet_cb); }

    Future<uint8_t> discover_hub(uint8_t *out_mac_addr);

private:
    NowIo() = default;

    void _on_message_received(const EspNowMessage &message);

    void _fill_packet_data(uint8_t *out_packet, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);
    NowPacket _process_message(const EspNowMessage &message);

    Future<uint8_t> _discover_hub_channel(uint8_t channel, uint8_t *out_mac_addr);
};

template<typename T, typename>
Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items) {
    return send(mac_addr, type, items.size(), (uint8_t *) items.data(), sizeof(T) * items.size());
}

template<typename T, size_t Count, typename>
Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]) {
    return send(mac_addr, type, Count, (uint8_t *) items, sizeof(T) * Count);
}

template<typename T, typename>
Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type, const T &item) {
    return send(mac_addr, type, 1, (uint8_t *) &item, sizeof(T));
}

template<typename T, typename>
Future<NowPacket> NowIo::request(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items) {
    return request(mac_addr, type, items.size(), (uint8_t *) items.data(), sizeof(T) * items.size());
}

template<typename T, size_t Count, typename>
Future<NowPacket> NowIo::request(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]) {
    return request(mac_addr, type, Count, (uint8_t *) items, sizeof(T) * Count);
}

template<typename T, typename>
Future<NowPacket> NowIo::request(const uint8_t *mac_addr, uint8_t type, const T &item) {
    return request(mac_addr, type, 1, (uint8_t *) &item, sizeof(T));
}

template<typename T, typename>
Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items) {
    return respond(id, mac_addr, type, (uint8_t *) items.data(), sizeof(T) * items.size());
}

template<typename T, size_t Count, typename>
Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]) {
    return respond(id, mac_addr, type, Count, (uint8_t *) items, sizeof(T) * Count);
}

template<typename T, typename>
Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, const T &item) {
    return respond(id, mac_addr, type, 1, (uint8_t *) &item, sizeof(T));
}

#endif
