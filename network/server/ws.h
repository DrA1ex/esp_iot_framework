#pragma once

#include <AsyncWebSocket.h>

#include "base.h"
#include "../protocol/packet_handler.h"

#include "../web.h"
#include "../../misc/circular_buffer.h"

#ifndef WS_MAX_PACKET_SIZE
#define WS_MAX_PACKET_SIZE                      (260u)
#endif

#ifndef WS_MAX_PACKET_QUEUE
#define WS_MAX_PACKET_QUEUE                     (10u)
#endif

struct WebSocketRequest {
    uint32_t client_id = 0;
    size_t size = 0;
    uint8_t data[WS_MAX_PACKET_SIZE] = {};
};

template<typename ApplicationT, typename = std::enable_if_t<std::is_base_of_v<
        AbstractApplication<typename ApplicationT::ConfigT, typename ApplicationT::MetaPropT>, ApplicationT>>>
class WebSocketServer : ServerBase<ApplicationT> {
    using PropEnumT = typename ApplicationT::PropEnumT;
    using PacketEnumT = typename ApplicationT::PacketEnumT;

    using PacketHandlerT = PacketHandler<ApplicationT>;

    CircularBuffer<WebSocketRequest, WS_MAX_PACKET_QUEUE> _request_queue;

    PacketHandlerT &_handler;
    const char *_path;
    AsyncWebSocket _ws;

public:
    explicit WebSocketServer(ApplicationT &app, PacketHandlerT &handler, const char *path = "/ws");

    void begin(WebServer &server);

    virtual void handle_incoming_data() override;

protected:
    void on_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    void notify_clients(uint32_t sender_id, PacketEnumT type);
    void notify_clients(uint32_t sender_id, PacketEnumT type, const void *data, uint8_t size);

    template<typename T>
    void notify_clients(uint32_t sender_id, PacketEnumT type, const T &value);

private:
    void _send_response(uint32_t client_id, uint16_t request_id, const Response &response);
    void _handle_notification(void *sender, PropEnumT type, void *arg);
};


template<typename ApplicationT, typename C1>
WebSocketServer<ApplicationT, C1>::WebSocketServer(ApplicationT &app, PacketHandlerT &handler, const char *path) :
        ServerBase<ApplicationT>(app),
        _handler(handler), _path(path), _ws(path) {}


template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::begin(WebServer &server) {
    using namespace std::placeholders;
    auto event_handler = std::bind(&WebSocketServer::on_event, this, _1, _2, _3, _4, _5, _6);

    _ws.onEvent(event_handler);
    server.add_handler(&_ws);

    this->app().event_property_changed().subscribe(this,
                                                   std::bind(&WebSocketServer<ApplicationT, C1>::_handle_notification, this, _1, _2, _3));

    D_WRITE("WebSocket server listening on path: ");
    D_PRINT(_path);
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::handle_incoming_data() {
    _ws.cleanupClients();

    while (_request_queue.can_pop()) {
        auto &request = *_request_queue.pop();

        auto parsingResponse = _handler.parse_packet(request.data, request.size);

        Response response;
        if (parsingResponse.success) {
            response = _handler.handle_packet_data(request.client_id, parsingResponse.packet);
        } else {
            response = parsingResponse.response;
        }

        return _send_response(request.client_id, parsingResponse.request_id, response);
    }
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::on_event(AsyncWebSocket *,
                                                 AsyncWebSocketClient *client,
                                                 AwsEventType type,
                                                 void *,
                                                 uint8_t *data,
                                                 size_t len) {

    switch (type) {
        case WS_EVT_CONNECT:
            D_PRINTF("WebSocket client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
            break;

        case WS_EVT_DISCONNECT:
            D_PRINTF("WebSocket client #%u disconnected\r\n", client->id());
            break;

        case WS_EVT_DATA: {
            D_PRINTF("WebSocket received packet, size: %u\r\n", len);

            if (len == 0) {
                _send_response(client->id(), 0, Response::code(ResponseCode::PACKET_LENGTH_EXCEEDED));
                return;
            }

            if (len > WS_MAX_PACKET_SIZE) {
                D_PRINTF("WebSocket packet dropped. Max packet size %ui\r\n, but received %u", WS_MAX_PACKET_SIZE, len);
                _send_response(client->id(), 0, Response::code(ResponseCode::PACKET_LENGTH_EXCEEDED));
                return;
            }

            if (!_request_queue.can_acquire()) {
                D_PRINT("WebSocket packet dropped. Queue is full");
                _send_response(client->id(), 0, Response::code(ResponseCode::TOO_MANY_REQUEST));
                return;
            }

            auto &request = *_request_queue.acquire();

            request.client_id = client->id();
            request.size = len;
            memcpy(request.data, data, len);

            break;
        }

        default:
            break;
    }
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::notify_clients(uint32_t sender_id, PacketEnumT type) {
    notify_clients(sender_id, type, nullptr, 0);
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::notify_clients(uint32_t sender_id, PacketEnumT type, const void *data, uint8_t size) {
    uint8_t message[sizeof(PacketHeader<PacketEnumT>) + size];

    (*(PacketHeader<PacketEnumT> *) message) = PacketHeader<PacketEnumT>{PACKET_SIGNATURE, 0, type, size};
    mempcpy(message + sizeof(PacketHeader<PacketEnumT>), data, size);

    D_PRINTF("WebSocket send notification: %s, total size: %u, data size: %u\r\n", __debug_enum_str(type), sizeof(message), size);

    for (auto &client: _ws.getClients()) {
        if (sender_id == client->id()) continue;

        VERBOSE(D_PRINTF("Websocket send notification to client: %u\r\n", client->id()));
        _ws.binary(client->id(), message, sizeof(message));
    }
}

template<typename ApplicationT, typename C1>
template<typename T>
void WebSocketServer<ApplicationT, C1>::notify_clients(uint32_t sender_id, PacketEnumT type, const T &value) {
    D_PRINTF("WebSocket send value message size: %u\r\n", sizeof(value));

    notify_clients(sender_id, type, &value, sizeof(value));
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::_send_response(uint32_t client_id, uint16_t request_id, const Response &response) {
    auto header = PacketHeader<PacketEnumT>{
            .signature = PACKET_SIGNATURE,
            .request_id = request_id
    };

    const void *data;

    switch (response.type) {
        //TODO: Separate type for response
        case ResponseType::CODE:
            header.type = (PacketEnumT) SystemPacketTypeEnum::RESPONSE_STRING;
            data = (void *) response.code_string();
            header.size = strlen((const char *) data);
            break;

        case ResponseType::STRING:
            header.type = (PacketEnumT) SystemPacketTypeEnum::RESPONSE_STRING;
            header.size = strlen(response.body.str);
            data = (void *) response.body.str;
            break;

        case ResponseType::BINARY:
            if (response.body.buffer.size > 255) {
                D_PRINTF("WebSocket response size too long: %u", response.body.buffer.size);
                return _send_response(client_id, request_id, Response::code(ResponseCode::INTERNAL_ERROR));
            }

            header.type = (PacketEnumT) SystemPacketTypeEnum::RESPONSE_BINARY;
            header.size = response.body.buffer.size;
            data = (void *) response.body.buffer.data;
            break;

        default:
            D_PRINTF("WebSocket unknown response type %u", (uint8_t) response.type);
            return _send_response(client_id, request_id, Response::code(ResponseCode::INTERNAL_ERROR));
    }


    uint8_t response_data[sizeof(header) + header.size];
    memcpy(response_data, &header, sizeof(header));
    memcpy(response_data + sizeof(header), data, header.size);

    _ws.binary(client_id, response_data, sizeof(response_data));
}

template<typename ApplicationT, typename C1>
void WebSocketServer<ApplicationT, C1>::_handle_notification(void *, PropEnumT type, void *arg) {
    const auto &prop_meta = this->app().property_meta();

    const auto prop_iterator = prop_meta.find(type);
    if (prop_iterator == prop_meta.cend()) {
        D_PRINTF("WebSocket unsupported notification type %s\r\n", __debug_enum_str(type));
        return;
    }

    const auto &prop = prop_iterator->second;
    auto client_id = arg ? *(uint32_t *) arg : 0;

    const auto &meta = prop[0];

    VERBOSE(
            D_PRINTF("WebSocket preparing notification data for %s, size: %u, offset: %u\r\n",
                     __debug_enum_str(type), meta.value_size, meta.value_offset)
    );

    // Copy data to avoid unaligned memory access
    uint8_t data[meta.value_size];
    memcpy(data, (uint8_t *) &this->app().config() + meta.value_offset, meta.value_size);

    if (prop.size() == 1) {
        notify_clients(client_id, meta.packet_type, data, meta.value_size);
    } else {
        if (meta.value_size != 1) {
            D_PRINTF("WebSocket unsupported notification for trigger type %s. Expected size 1 byte, but got %u\r\n",
                     __debug_enum_str(type), meta.value_size);
            return;
        }

        bool trigger = *(bool *) data;
        notify_clients(client_id, trigger ? meta.packet_type : prop[1].packet_type);
    }
}
