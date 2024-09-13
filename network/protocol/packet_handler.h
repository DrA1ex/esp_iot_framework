#pragma once

#include "../../base/application.h"
#include "../protocol/binary.h"
#include "../protocol/type.h"

template<typename ApplicationT, typename = std::enable_if_t<std::is_base_of_v<
        AbstractApplication<typename ApplicationT::ConfigT, typename ApplicationT::MetaPropT>, ApplicationT>>>
class PacketHandler {
public:
    using PropEnumT = typename ApplicationT::PropEnumT;
    using PacketEnumT = typename ApplicationT::PacketEnumT;
    using PacketHeaderT = PacketHeader<PacketEnumT>;

    explicit PacketHandler(ApplicationT &app);
    virtual ~PacketHandler() = default;

    virtual Response handle_packet_buffer(uint32_t client_id, const uint8_t *buffer, uint16_t length);
    virtual Response handle_packet_data(uint32_t client_id, const Packet<PacketEnumT> &packet);

    virtual Response handle_parameter_update(PacketHeaderT *header, const void *data);
    virtual Response handle_system_command(PacketHeader<PacketEnumT> *header, const void *data);

    inline PacketParsingResponse<PacketEnumT> parse_packet(const uint8_t *buffer, uint16_t length) {
        return _protocol.parse_packet(buffer, length);
    }

private:
    ApplicationT &_app;
    BinaryProtocol<PacketEnumT> _protocol;

protected:
    inline ApplicationT &app() { return _app; }
    inline BinaryProtocol<PacketEnumT> &protocol() { return _protocol; }

    virtual void send_notification(uint32_t client_id, const Packet<PacketEnumT> &packet);
};

template<typename ApplicationT, typename S1>
PacketHandler<ApplicationT, S1>::PacketHandler(ApplicationT &app) : _app(app), _protocol() {}


template<typename ApplicationT, typename S1>
Response PacketHandler<ApplicationT, S1>::handle_packet_buffer(uint32_t client_id, const uint8_t *buffer, uint16_t length) {
    const auto parse_response = parse_packet(buffer, length);
    if (!parse_response.success) return parse_response.response;

    return handle_packet_data(client_id, parse_response.packet);
}

template<typename ApplicationT, typename S1>
Response PacketHandler<ApplicationT, S1>::handle_packet_data(uint32_t client_id, const Packet<PacketEnumT> &packet) {
    if ((int) packet.header->type >= 0xf0) {
        return handle_system_command(packet.header, packet.data);
    }

    auto response = handle_parameter_update(packet.header, packet.data);
    if (response.is_ok()) send_notification(client_id, packet);

    return response;
}

template<typename ApplicationT, typename S1>
Response PacketHandler<ApplicationT, S1>::handle_parameter_update(PacketHeaderT *header, const void *data) {
    auto meta_iter = _app.packet_meta().find(header->type);

    if (meta_iter == _app.packet_meta().cend()) {
        D_PRINTF("Received unsupported parameter: %u (%s)\r\n", _protocol.to_underlying(header->type), __debug_enum_str(header->type));
        return Response::code(ResponseCode::BAD_COMMAND);
    }
    auto &meta = meta_iter->second;
    return _protocol.update_parameter_value(((uint8_t *) (&_app.config())) + meta.value_offset, meta.value_size, *header, data);
}

template<typename ApplicationT, typename S1>
Response PacketHandler<ApplicationT, S1>::handle_system_command(PacketHeader<PacketEnumT> *header, const void *data) {
    if ((SystemPacketTypeEnum) header->type == SystemPacketTypeEnum::GET_CONFIG) {
        return _protocol.serialize(app().config());
    } else if ((SystemPacketTypeEnum) header->type == SystemPacketTypeEnum::RESTART) {
        app().restart();
        return Response::ok();
    }

    return Response::code(ResponseCode::BAD_COMMAND);
}

template<typename ApplicationT, typename S1>
void PacketHandler<ApplicationT, S1>::send_notification(uint32_t client_id, const Packet<PacketEnumT> &packet) {
    const auto &packet_meta = app().packet_meta();
    auto meta_iterator = packet_meta.find(packet.header->type);
    if (meta_iterator != packet_meta.end()) {
        app().notify_property_changed(this, meta_iterator->second.property, &client_id);
    } else {
        D_PRINTF("Handler: Unsupported notification packet type: %s\r\n", __debug_enum_str(packet.header->type));
    }
}
