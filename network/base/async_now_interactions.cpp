#ifdef ARDUINO_ARCH_ESP32

#include "async_now_interactions.h"

AsyncEspNowInteraction AsyncEspNowInteraction::_instance = {};
const uint8_t AsyncEspNowInteraction::BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

bool AsyncEspNowInteraction::begin() {
    if (_initialized) return false;

    auto ok = _async_now.begin();
    if (!ok) return false;

    _async_now.set_on_packet_cb([this](auto packet) { _on_packet_received(std::move(packet)); });

    _initialized = true;
    return true;
}

void AsyncEspNowInteraction::end() {
    if (!_initialized) return;

    set_on_message_cb(nullptr);
    _initialized = false;

    _async_now.end();
}

Future<EspNowSendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const char *str) {
    return send(mac_addr, (uint8_t *) str, strlen(str));
}

Future<EspNowSendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (!_initialized) return Future<EspNowSendResponse>::errored();

    return _send_impl(_id++, false, mac_addr, data, size);
}

Future<EspNowMessage> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const char *str) {
    return request(mac_addr, (uint8_t *) str, strlen(str));
}

Future<EspNowMessage> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (!_initialized) return Future<EspNowMessage>::errored();

    return _request_impl(_id++, mac_addr, data, size);
}

Future<void> AsyncEspNowInteraction::respond(uint8_t id, const uint8_t *mac_addr, const char *str) {
    return respond(id, mac_addr, (uint8_t *) str, strlen(str));
}

Future<void> AsyncEspNowInteraction::respond(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (!_initialized) return Future<EspNowSendResponse>::errored();

    return _send_impl(id, true, mac_addr, data, size);
}

Future<uint8_t> AsyncEspNowInteraction::discover_peer_channel(const uint8_t *mac_addr) {
    D_WRITE("EspNowInteraction: Discovering peer channel: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    if (!_async_now.is_peer_exists(mac_addr)) _async_now.register_peer(mac_addr);

    auto channel = std::make_shared<uint8_t>(0);
    return PromiseBase::sequential<uint8_t>(
        _configure_peer_channel(mac_addr, (*channel)++),
        [channel](auto future) { return !future.success() && *channel < 14; },
        [this, mac_addr, channel=std::move(channel)](auto) {
            return _configure_peer_channel(mac_addr, (*channel)++);
        }
    );
}

Future<EspNowSendResponse> AsyncEspNowInteraction::_send_impl(
    uint8_t id, bool is_response, const uint8_t *mac_addr, const uint8_t *data, uint16_t size
) {
    if (data == nullptr || size == 0) {
        D_PRINT("EspNowInteraction: data missing");
        return Future<EspNowSendResponse>::errored();
    }
    if (size > ESP_NOW_INTERACTION_MAX_DATA_LENGTH) {
        D_PRINTF("EspNowInteraction: sending data to big: %i (max %i) \r\n", size, ESP_NOW_INTERACTION_MAX_DATA_LENGTH);
        return Future<EspNowSendResponse>::errored();
    }

    const uint8_t total_packets_count = size / ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH
            + (size % ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH ? 1 : 0);

    const uint8_t *data_ptr = data;
    uint8_t index = 0;

    std::vector<Future<void>> futures;
    uint16_t remaining = size;
    while (remaining > 0) {
        const auto packet_data_size = (uint8_t) std::min<uint16_t>(remaining, ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH);
        uint8_t packet[ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH + packet_data_size];

        auto *header = (EspNowInteractionPacketHeader *) packet;
        *header = {
            .id = id,
            .is_response = is_response,
            .index = index++,
            .count = total_packets_count,
            .size = packet_data_size,
        };

        memcpy(packet + ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH, data_ptr, packet_data_size);

        auto future = _async_now.send(mac_addr, packet, sizeof(packet));
        futures.push_back(std::move(future));

        D_PRINTF("EspNowInteraction: sending message %i packet %i/%i, size %i\r\n",
            header->id, header->index + 1, header->count, header->size);

        data_ptr += packet_data_size;
        remaining -= packet_data_size;
    }

    return PromiseBase::all(futures).then<EspNowSendResponse>([id](const FutureBase &) {
        return EspNowSendResponse {.id = id};
    });
}

Future<EspNowMessage> AsyncEspNowInteraction::_request_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (auto it = _requests.find(id); it != _requests.end()) {
        D_PRINTF("EspNowInteraction: request %i already exist. Cancelling...\r\n", id);

        // Acquire shared pointer to avoid destruction right after ::erase()
        auto existing_promise = it->second;
        _requests.erase(it);

        existing_promise->set_error();
    }

    auto promise = Promise<EspNowMessage>::create();
    _requests[id] = promise;

    auto send_future = _send_impl(id, false, mac_addr, data, size);
    return send_future.then<EspNowMessage>([this, promise = std::move(promise)](const auto &future) {
        auto response = future.result();
        VERBOSE(D_PRINTF("EspNowInteraction: request %i sent. Waiting for response...\r\n", response.id));
        return Future {promise};
    }).on_error([this, id](auto future) {
        _requests.erase(id);
        return future;
    });
}

Future<uint8_t> AsyncEspNowInteraction::_configure_peer_channel(const uint8_t *mac_addr, uint8_t channel) {
    if (channel > 14 || !_async_now.change_channel(channel)) return Future<uint8_t>::errored();

    D_PRINTF("EspNowInteraction: Trying channel %i...\r\n", channel + 1);

    uint8_t data[1] {};
    return send(mac_addr, data, sizeof(data))
           .then<uint8_t>([=](auto) {
               D_PRINTF("EspNowInteraction: Channel %i is valid!\r\n", channel + 1);
               return channel;
           }).on_error([=](auto future) {
               D_PRINTF("EspNowInteraction: Channel %i isn't valid!\r\n", channel + 1);
               return future;
           });
}

void AsyncEspNowInteraction::_on_packet_received(EspNowPacket packet) {
    if (packet.size < sizeof(EspNowInteractionPacketHeader)) {
        D_PRINT("EspNowInteraction: received message is too small");
        return;
    }

    auto *header = (EspNowInteractionPacketHeader *) packet.data.get();

    if (header->count != 1 && header->index < header->size - 1 && header->size != ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH) {
        D_PRINTF("EspNowInteraction: received ill-formed message id %i packet %i\r\n", header->id, header->index);
        return;
    }

    D_PRINTF("EspNowInteraction: received message %i packet %i/%i, size %i\r\n",
        header->id, header->index + 1, header->count, header->size);

    EspNowMessageKey message_key = {
        .fields = {
            .id = header->id,
            .is_response = header->is_response
        },
    };
    memcpy(message_key.fields.mac_addr, packet.mac_addr, sizeof(packet.mac_addr));

    auto message_it = _messages.find(message_key.u64);
    if (message_it == _messages.end()) {
        EspNowMessage new_message = {
            .id = header->id,
            .received_count = 0,
            .parts_count = header->count,
            .size = 0,
            .data = std::shared_ptr<uint8_t[]>(new uint8_t[
                header->count == 1
                    ? header->size
                    : header->count * ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH
            ]),
        };

        memcpy(new_message.mac_addr, packet.mac_addr, sizeof(new_message.mac_addr));
        _messages[message_key.u64] = std::move(new_message);
    }

    auto &message = _messages[message_key.u64];
    message.size += header->size;
    message.received_count++;

    memcpy(message.data.get() + ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH * header->index,
        packet.data.get() + ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH, header->size);

    if (message.received_count != message.parts_count) return;

    //TODO: unique request_id for each peer?
    if (auto it = _requests.find(header->id); header->is_response && it != _requests.end()) {
        D_PRINTF("EspNowInteraction: received message response id %i\r\n", message.id);

        // Acquire shared pointer to avoid destruction right after ::erase()
        auto promise = std::move(it->second);
        _requests.erase(it);

        promise->set_success(message);
    } else if (header->is_response) {
        D_PRINTF("EspNowInteraction: received unexpected response id %i\r\n", message.id);
    } else {
        D_PRINTF("EspNowInteraction: received message id %i, size %i\r\n", message.id, message.size);

        if (_on_message_cb) {
            _on_message_cb(message);
        }
    }

    _messages.erase(message_key.u64);
}

#endif
