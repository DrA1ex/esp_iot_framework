#pragma once

#include <algorithm>
#include <memory.h>
#include <type_traits>

#include "../../debug.h"
#include "./type.h"


#ifndef PACKET_SIGNATURE
#define PACKET_SIGNATURE                        ((uint16_t) 0xDABA)
#endif

template<typename PacketEnumT, typename = std::enable_if_t<std::is_enum_v<PacketEnumT>>>
class BinaryProtocol {
public:
    PacketParsingResponse<PacketEnumT> parse_packet(const uint8_t *buffer, uint8_t length);

    template<typename T, typename = std::enable_if<std::is_enum<T>::value || std::is_integral<T>::value>>
    Response update_parameter_value(T *parameter, const PacketHeader<PacketEnumT> &header, const void *data);

    Response update_parameter_value(void *pointer, uint8_t size, const PacketHeader<PacketEnumT> &header, const void *data);

    template<typename T, uint8_t N, typename = std::enable_if_t<std::is_enum<T>::value || std::is_integral<T>::value>>
    Response update_parameter_value_array(T (&array)[N], const PacketHeader<PacketEnumT> &header, const void *data);

    Response update_string_value(char *str, uint8_t max_size, const PacketHeader<PacketEnumT> &header, const void *data);

    template<uint8_t StrSize>
    Response
    update_string_list_value(char destination[][StrSize], uint8_t max_count, const PacketHeader<PacketEnumT> &header, const void *data);

    template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
    Response serialize(const T &obj);

    template<typename E, typename = std::enable_if_t<std::is_trivial_v<E>>>
    constexpr auto to_underlying(E e) noexcept;

protected:
    Response _update_value(uint8_t *target, uint8_t target_size, PacketEnumT type, const void *data, uint8_t data_size);
};

template<typename PacketT, typename S1>
template<typename E, typename>
constexpr auto BinaryProtocol<PacketT, S1>::to_underlying(E e) noexcept {
    if constexpr (std::is_enum<E>::value) {
        return static_cast<typename std::underlying_type<E>::type>(e);
    } else {
        return static_cast<E>(e);
    }
}

template<typename PacketT, typename S1>
template<typename T, typename>
Response BinaryProtocol<PacketT, S1>::serialize(const T &obj) {
    return Response{ResponseType::BINARY, {.buffer = {.size = sizeof(obj), .data=(uint8_t *) &obj}}};
}

template<typename PacketT, typename S1>
template<uint8_t StrSize>
Response BinaryProtocol<PacketT, S1>::update_string_list_value(
        char (*destination)[StrSize], const uint8_t max_count, const PacketHeader<PacketT> &header, const void *data) {
    if (header.size < 2) {
        D_PRINTF("Unable to update string list, bad size. Got %u, expected at least %u\r\n", header.size, 2);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    uint8_t dst_index;
    memcpy(&dst_index, data, sizeof(dst_index));
    size_t offset = 1;

    if (dst_index >= max_count) {
        D_PRINTF("Unable to update string list, bad destination offset. Got %u, but limit is %u\r\n", dst_index, max_count - 1);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    const char *input = (const char *) data + offset;

    size_t updated_count = 0;
    while (offset < header.size) {
        if (dst_index >= max_count) {
            D_PRINT("Unable to finish update. Received too many values");
            break;
        }

        const size_t length = strnlen(input, header.size - offset);
        if (length > StrSize) D_PRINTF("Value at %u will be truncated. Read size %u, but limit is %u\r\n", dst_index, length, StrSize);

        memcpy(destination[dst_index], input, std::min((uint8_t) length, StrSize));
        if (length < StrSize) destination[dst_index][length] = '\0';

        D_PRINTF("Update #%u: %.*s (%u)\r\n", dst_index, StrSize, destination[dst_index], length);

        dst_index++;
        updated_count++;
        offset += length + 1;
        input += length + 1;
    }

    D_WRITE("Update string list ");
    D_WRITE(__debug_enum_str(header.type));
    D_PRINTF(" (Count: %i)\r\n", updated_count);

    return Response::ok();
}

template<typename PacketT, typename S1>
template<typename T, typename>
Response BinaryProtocol<PacketT, S1>::update_parameter_value(T *parameter, const PacketHeader<PacketT> &header, const void *data) {
    if (header.size != sizeof(T)) {
        D_PRINTF("Unable to update value, bad size. Got %u, expected %u\r\n", header.size, sizeof(T));
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    memcpy(parameter, data, sizeof(T));

    D_WRITE("Update parameter ");
    D_WRITE(__debug_enum_str(header.type));
    D_WRITE(" = ");

    // Copy to aligned memory to avoid unaligned memory access
    {
        uint8_t debug_data[sizeof(T)];
        memcpy(debug_data, data, sizeof(T));
        D_PRINT(to_underlying(*(T *) debug_data));
    }

    return Response::ok();
}

template<typename PacketT, typename S1>
PacketParsingResponse<PacketT> BinaryProtocol<PacketT, S1>::parse_packet(const uint8_t *buffer, uint8_t length) {
    D_WRITE("---- Packet body: ");
    D_PRINT_HEX(buffer, length);

    const auto header_size = sizeof(PacketHeader<PacketT>);
    if (length < header_size) {
        D_PRINTF("Wrong packet size. Expected at least: %u\r\n", header_size);

        return PacketParsingResponse<PacketT>::fail(Response::code(ResponseCode::PACKET_LENGTH_EXCEEDED));
    }

    auto *packet = (PacketHeader<PacketT> *) buffer;
    if (packet->signature != PACKET_SIGNATURE) {
        D_PRINTF("Wrong packet signature: %X\r\n", packet->signature);

        return PacketParsingResponse<PacketT>::fail(Response::code(ResponseCode::BAD_REQUEST), packet->request_id);
    }

    if (header_size + packet->size != length) {
        D_PRINTF("Wrong message length, expected: %u\r\n", header_size + packet->size);

        return PacketParsingResponse<PacketT>::fail(Response::code(ResponseCode::BAD_REQUEST), packet->request_id);
    }

    D_PRINTF("---- Packet type: %s\r\n", (int) packet->type < 0xf0
                                         ? __debug_enum_str(packet->type)
                                         : __debug_enum_str((SystemPacketTypeEnum) packet->type));

    D_PRINTF("---- Packet Request-ID: %u\r\n", packet->request_id);
    D_PRINTF("---- Packet Data-Size: %u\r\n", packet->size);

    const void *data = buffer + header_size;
    return PacketParsingResponse<PacketT>::ok({packet, data}, packet->request_id);
}

template<typename PacketT, typename S1>
Response BinaryProtocol<PacketT, S1>::update_string_value(
        char *str, uint8_t max_size, const PacketHeader<PacketT> &header, const void *data) {
    if (header.size > max_size) {
        D_PRINTF("Unable to update value, data too long. Got %u, but limit is %u\r\n", header.size, max_size);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    memcpy(str, data, header.size);
    if (header.size < max_size) str[header.size] = '\0';

    D_WRITE("Update parameter ");
    D_WRITE(to_underlying(header.type));
    D_PRINTF(" = %.*s\r\n", max_size, str);

    return Response::ok();
}

template<typename PacketT, typename S1>
Response BinaryProtocol<PacketT, S1>::update_parameter_value(
        void *pointer, uint8_t size, const PacketHeader<PacketT> &header, const void *data) {
    if (header.size != size) {
        D_PRINTF("Unable to update value, bad size. Got %u, expected %u\r\n", header.size, size);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    memcpy(pointer, data, size);

    D_WRITE("Update parameter ");
    D_WRITE(__debug_enum_str(header.type));
    D_WRITE(" = ");

    D_PRINT_HEX((uint8_t *) pointer, size);

    return Response::ok();
}

template<typename PacketEnumT, typename S1>
template<typename T, uint8_t N, typename>
Response
BinaryProtocol<PacketEnumT, S1>::update_parameter_value_array(T (&array)[N], const PacketHeader<PacketEnumT> &header, const void *data) {
    const auto expected_size = sizeof(T) + sizeof(N);
    if (header.size != expected_size) {
        D_PRINTF("Unable to update array value, bad size. Got %u, expected %u\r\n", header.size, expected_size);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    decltype(N) index;
    memcpy(&index, data, sizeof(N));

    if (index >= N) {
        D_PRINTF("Unable to update array value, bad index. Got %u, but array size is %u\r\n", index, N);
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    memcpy((array + index), (const uint8_t *) data + sizeof(N), sizeof(T));

    D_WRITE("Update parameter ");
    D_PRINTF("%s[%u]", __debug_enum_str(header.type), index);
    D_WRITE(" = ");

    D_PRINT_HEX((uint8_t *) (array + index), sizeof(T));

    return Response::ok();
}