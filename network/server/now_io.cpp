#ifdef ARDUINO_ARCH_ESP32

#include "now_io.h"

#include "../../async/system_timer.h"

NowIo NowIo::_instance {};

bool NowIo::begin() {
    if (!_interaction.begin()) return false;

    _interaction.set_on_message_cb([this](auto message) { _on_message_received(message); });
    return true;
}

void NowIo::end() {
    _interaction.end();
}

Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type) {
    return send(mac_addr, type, 0, nullptr, 0);
}

Future<EspNowMessage> NowIo::request(const uint8_t *mac_addr, uint8_t type) {
    return request(mac_addr, type, 0, nullptr, 0);
}

Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.send(mac_addr, packet, sizeof(packet));
}

Future<EspNowMessage> NowIo::request(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.request(mac_addr, packet, sizeof(packet));
}

Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type) {
    return respond(id, mac_addr, type, 0, nullptr, 0);
}

Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.respond(id, mac_addr, packet, sizeof(packet));
}

Future<uint8_t> NowIo::discover_hub(uint8_t *out_mac_addr) {
    D_PRINT("NowIo: Discovering hub...");

    auto channel = std::make_shared<uint8_t>(0);
    auto discovery_future = PromiseBase::sequential<uint8_t>(
        _discover_hub_channel((*channel)++, out_mac_addr),
        [channel](auto future) { return !future.success() && *channel < 14; },
        [this, out_mac_addr, channel](auto) {
            return _discover_hub_channel((*channel)++, out_mac_addr);
        }
    );

    // Verify hub addr and channel
    return discovery_future
        .then<void>([this, out_mac_addr](auto) {
            D_PRINT("NowIo: Verifying hub...");
            return send(out_mac_addr, (uint8_t) SpecialPacketTypes::PING);
        }).then<uint8_t>([discovery_future](auto) {
            D_PRINT("NowIo: Hub verified...");
            return discovery_future;
        });
}

void NowIo::_on_message_received(const EspNowMessage &message) {
    if (_on_packet_cb) {
        auto packet = NowPacket::parse(message);
        D_PRINT("NowIo: received package");
        D_PRINTF("\t- Id: %u\r\n", packet.id);
        D_PRINTF("\t- Type: %u\r\n", packet.type);
        D_PRINTF("\t- Count: %u\r\n", packet.count);
        D_PRINTF("\t- Data-Size: %u\r\n", packet.size);
        D_WRITE("\t- Sender:");
        D_PRINT_HEX(message.mac_addr, sizeof(message.mac_addr));

        _on_packet_cb(std::move(packet));
    } else {
        D_WRITE("NowIo: received package from: ");
        D_PRINT_HEX(message.mac_addr, sizeof(message.mac_addr));
    }
}

void NowIo::_fill_packet_data(uint8_t *out_packet, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    auto *header = (NowPacketHeader *) out_packet;
    *header = {.type = type, .count = count};
    if (data && count) memcpy(out_packet + sizeof(NowPacketHeader), data, size);
}

Future<uint8_t> NowIo::_discover_hub_channel(uint8_t channel, uint8_t *out_mac_addr) {
    if (channel > 14 || !AsyncEspNow::instance().change_channel(channel)) return Future<uint8_t>::errored();

    D_PRINTF("NowIo: Trying to discover hub at channel %i...\r\n", channel + 1);

    auto request_future = request(AsyncEspNowInteraction::BROADCAST_MAC, (uint8_t) SpecialPacketTypes::DISCOVERY);
    auto delay_future = SystemTimer::delay(100);

    return PromiseBase::any({request_future, delay_future})
        .then<uint8_t>([=](auto) {
            if (!request_future.finished() || !request_future.success()) return Future<uint8_t>::errored();

            D_PRINTF("NowIo: Hub respond at channel %i!\r\n", channel + 1);
            memcpy(out_mac_addr, request_future.result().mac_addr, sizeof(AsyncEspNowInteraction::BROADCAST_MAC));
            return Future<uint8_t>::successful(channel);
        }).on_error([=](auto future) {
            D_PRINTF("NowIo: Hub doesn't respond on channel %i\r\n", channel + 1);
            return future;
        });
}


NowPacket NowPacket::parse(const EspNowMessage &message) {
    NowPacket result {};

    auto *header = (NowPacketHeader *) message.data.get();
    result.id = message.id;
    result.type = header->type;
    result.count = header->count;

    memcpy(result.mac_addr, message.mac_addr, sizeof(result.mac_addr));
    result.data = message.data.get() + sizeof(NowPacketHeader);
    result.size = message.size - sizeof(NowPacketHeader);
    result._message_data = message.data;

    return result;
}

#endif
