#pragma once

#include <cstdint>
#include <type_traits>

#include "../../utils/enum.h"

enum class ResponseType : uint8_t {
    CODE,
    STRING,
    BINARY,
};

enum class ResponseCode : uint8_t {
    OK,

    BAD_REQUEST,
    BAD_COMMAND,

    TOO_MANY_REQUEST,
    PACKET_LENGTH_EXCEEDED,

    INTERNAL_ERROR
};

MAKE_ENUM(SystemPacketTypeEnum, uint8_t,
          RESPONSE_STRING, 0xf0,
          RESPONSE_BINARY, 0xf1,

          GET_CONFIG, 0xfa,
          RESTART, 0xfb,
)

struct Response {
    ResponseType type;

    union {
        ResponseCode code;
        const char *str;

        struct {
            uint16_t size;
            uint8_t *data;
        } buffer;
    } body;

    [[nodiscard]] inline bool is_ok() const { return type != ResponseType::CODE || body.code == ResponseCode::OK; }

    inline static Response ok() {
        return code(ResponseCode::OK);
    };

    inline static Response code(ResponseCode code) {
        return Response{.type = ResponseType::CODE, .body = {.code = code}};
    };

    [[nodiscard]] const char *code_string() const {
        if (type != ResponseType::CODE) return nullptr;

        switch (body.code) {
            case ResponseCode::OK:
                return "OK";

            case ResponseCode::BAD_REQUEST:
                return "BAD REQUEST";

            case ResponseCode::BAD_COMMAND:
                return "BAD COMMAND";

            case ResponseCode::TOO_MANY_REQUEST:
                return "TOO MANY REQUESTS";

            case ResponseCode::PACKET_LENGTH_EXCEEDED:
                return "PACKET LENGTH EXCEEDED";

            case ResponseCode::INTERNAL_ERROR:
            default:
                return "INTERNAL ERROR";
        }
    }
};

template<typename PacketEnumT>
struct __attribute__ ((packed))  PacketHeader {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");

    uint16_t signature;
    uint16_t request_id;
    PacketEnumT type;
    uint16_t size;
};

template<typename PacketEnumT>
struct Packet {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");

    PacketHeader<PacketEnumT> *header;
    const void *data;
};

template<typename PacketEnumT>
struct PacketParsingResponse {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");

    bool success;
    uint16_t request_id;

    union {
        Packet<PacketEnumT> packet;
        Response response;
    };

    static PacketParsingResponse ok(Packet<PacketEnumT> &&packet, uint16_t request_id = 0) {
        return PacketParsingResponse{.success = true, .request_id = request_id, .packet = packet};
    }

    static PacketParsingResponse fail(Response &&response, uint16_t request_id = 0) {
        return PacketParsingResponse{.success = false, .request_id = request_id, .response = response};
    }
};