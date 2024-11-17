#ifdef ARDUINO_ARCH_ESP32

#include "now_io.h"

#include <lib/async/system_timer.h>

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

Future<NowPacket> NowIo::request(const uint8_t *mac_addr, uint8_t type) {
    return request(mac_addr, type, 0, nullptr, 0);
}

Future<void> NowIo::send(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.send(mac_addr, packet, sizeof(packet));
}

Future<NowPacket> NowIo::request(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    auto request_future = _interaction.request(mac_addr, packet, sizeof(packet));
    return request_future.then<NowPacket>([this](auto f) {
        return _process_message(f.result());
    });
}

Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type) {
    return respond(id, mac_addr, type, 0, nullptr, 0);
}

Future<void> NowIo::respond(uint8_t id, const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(NowPacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.respond(id, mac_addr, packet, sizeof(packet));
}

Future<void> NowIo::ping(const uint8_t *mac_addr) {
    return request(mac_addr, (uint8_t) SpecialPacketTypes::PING)
            .then<void>([](auto f) {
                auto response = f.result();
                auto valid = response.type == (uint8_t) SpecialPacketTypes::SYSTEM_RESPONSE
                        && response.count == 0
                        && response.size == 0;

                return valid ? Future<void>::successful()
                           : Future<void>::errored();
            });
}

Future<void> NowIo::discovery(uint8_t *out_mac_addr) {
    return request(AsyncEspNowInteraction::BROADCAST_MAC, (uint8_t) SpecialPacketTypes::DISCOVERY)
            .then<void>([out_mac_addr](auto f) {
                auto response = f.result();
                auto valid = response.type == (uint8_t) SpecialPacketTypes::SYSTEM_RESPONSE
                        && response.count == 0
                        && response.size == 0;

                if (!valid) return Future<void>::errored();

                memcpy(out_mac_addr, response.mac_addr, sizeof(response.mac_addr));
                return Future<void>::successful();
            });
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
           .then<void>([this, mac_addr = out_mac_addr](auto) {
               D_PRINT("NowIo: Verifying hub...");
               return ping(mac_addr);
           }).then<uint8_t>([discovery_future](auto) {
               D_PRINT("NowIo: Hub verified...");
               return discovery_future;
           });
}

void NowIo::_on_message_received(const EspNowMessage &message) {
    if (_on_packet_cb) {
        auto packet = _process_message(message);
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

NowPacket NowIo::_process_message(const EspNowMessage &message) {
    auto packet = NowPacket::parse(message);
    D_PRINT("NowIo: received package");
    D_PRINTF("\t- Id: %u\r\n", packet.id);
    D_PRINTF("\t- Type: %u\r\n", packet.type);
    D_PRINTF("\t- Count: %u\r\n", packet.count);
    D_PRINTF("\t- Data-Size: %u\r\n", packet.size);
    D_WRITE("\t- Sender:");
    D_PRINT_HEX(packet.mac_addr, sizeof(packet.mac_addr));

    return packet;
}

Future<uint8_t> NowIo::_discover_hub_channel(uint8_t channel, uint8_t *out_mac_addr) {
    if (channel > 14 || !AsyncEspNow::instance().change_channel(channel)) return Future<uint8_t>::errored();

    D_PRINTF("NowIo: Trying to discover hub at channel %i...\r\n", channel + 1);

    auto request_future = discovery(out_mac_addr);
    auto delay_future = SystemTimer::delay(100);

    return PromiseBase::any({request_future, delay_future})
           .then<uint8_t>([=](auto) {
               if (!request_future.finished() || !request_future.success()) return Future<uint8_t>::errored();
               D_PRINTF("NowIo: Hub respond at channel %i!\r\n", channel + 1);
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
