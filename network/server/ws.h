#pragma once

#include <map>
#include <AsyncWebSocket.h>

#include "../../debug.h"
#include "../../base/parameter.h"
#include "../../misc/circular_buffer.h"
#include "../../misc/notification_bus.h"
#include "../protocol/binary.h"
#include "../protocol/type.h"

#ifndef WS_MAX_PACKET_SIZE
#define WS_MAX_PACKET_SIZE                      (260u)
#endif

#ifndef WS_MAX_PACKET_BODY_SIZE
#define WS_MAX_PACKET_BODY_SIZE                 (4096u)
#endif

#ifndef WS_MAX_PACKET_QUEUE
#define WS_MAX_PACKET_QUEUE                     (10u)
#endif

struct WebSocketRequest {
    uint32_t client_id = 0;
    size_t size = 0;
    uint8_t data[WS_MAX_PACKET_SIZE] = {};
};

typedef std::function<void(const void *data, uint16_t size)> WebSocketCommand;

template<typename PacketEnumT>
class WebSocketServer {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");

    using PacketT = Packet<PacketEnumT>;

    std::map<PacketEnumT, WebSocketCommand> _commands;
    std::map<PacketEnumT, const AbstractParameter *> _data_requests;
    std::map<PacketEnumT, const AbstractParameter *> _notifications;
    std::map<PacketEnumT, AbstractParameter *> _parameters;
    std::map<const AbstractParameter *, PacketEnumT> _parameters_packet_type;

    CircularBuffer<WebSocketRequest, WS_MAX_PACKET_QUEUE> _request_queue;

    const char *_path;
    AsyncWebSocket _ws;
    uint32_t _client_count = 0;

    BinaryProtocol<PacketEnumT> _protocol;

public:
    explicit WebSocketServer(const char *path = "/ws");

    void begin(WebServer &server);
    void handle_connection();

    void register_command(PacketEnumT type, Command command);
    void register_command(PacketEnumT type, WebSocketCommand command);
    void register_data_request(PacketEnumT type, const AbstractParameter *parameter);
    void register_notification(PacketEnumT type, const AbstractParameter *parameter);
    void register_parameter(PacketEnumT type, AbstractParameter *parameter);

    void send_notification(PacketEnumT type);

protected:
    void on_event(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *, uint8_t *data, size_t len);
    void send_response(uint32_t client_id, uint16_t request_id, const Response &response);

    Response handle_packet_data(uint32_t client_id, PacketT packet);

private:
    template<typename T>
    void _notify_clients(uint32_t sender_id, PacketEnumT type, const T &value);
    void _notify_clients(uint32_t sender_id, PacketEnumT type);
    void _notify_clients(uint32_t sender_id, PacketEnumT type, const void *data, uint8_t size);

    void _process_notification(void *sender, const AbstractParameter *parameter);
};

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::send_notification(PacketEnumT type) {
    const AbstractParameter *param = nullptr;

    if (auto it_n = _notifications.find(type); it_n != _notifications.end()) {
        param = it_n->second;
    }

    if (auto it_p = _parameters.find(type); !param && it_p != _parameters.end()) {
        param = it_p->second;
    }

    if (!param) {
        D_PRINTF("WebSocket: unsupported notification type %s\r\n", __debug_enum_str(type));
        return;
    }

    _notify_clients(-1, type, param->get_value(), param->size());
}

template<typename PacketEnumT>
WebSocketServer<PacketEnumT>::WebSocketServer(const char *path) : _path(path), _ws(_path) {}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::begin(WebServer &server) {
    using namespace std::placeholders;
    auto event_handler = std::bind(&WebSocketServer::on_event, this, _1, _2, _3, _4, _5, _6);

    _ws.onEvent(event_handler);
    server.add_handler(&_ws);

    using namespace std::placeholders;
    NotificationBus::get().subscribe(std::bind(&WebSocketServer::_process_notification, this, _1, _2));

    D_WRITE("WebSocket: server listening on path: ");
    D_PRINT(_path);
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::handle_connection() {
    _ws.cleanupClients();

    while (_request_queue.can_pop()) {
        auto &request = *_request_queue.pop();

        auto parsing_response = _protocol.parse_packet(request.data, request.size);

        Response response = parsing_response.success
                            ? handle_packet_data(request.client_id, parsing_response.packet)
                            : parsing_response.response;

        return send_response(request.client_id, parsing_response.request_id, response);
    }
}

template<typename PacketEnumT>
Response WebSocketServer<PacketEnumT>::handle_packet_data(uint32_t client_id, PacketT packet) {
    if (auto cmd_it = _commands.find(packet.header->type); cmd_it != _commands.end()) {
        cmd_it->second(packet.data, packet.header->size);
        return Response::ok();
    } else if (auto data_it = _data_requests.find(packet.header->type); data_it != _data_requests.end()) {
        auto param = data_it->second;
        return Response{
            .type = ResponseType::BINARY,
            .body = {
                .buffer = {
                    .size = (uint16_t) param->size(),
                    .data = (uint8_t *) param->get_value()
                }
            }
        };
    } else if (auto param_it = _parameters.find(packet.header->type); param_it != _parameters.end()) {
        auto param = param_it->second;
        bool success = param->set_value(packet.data, packet.header->size);
        if (success) {
            D_PRINTF("WebSocket: set parameter %s = ", __debug_enum_str(packet.header->type));
            D_PRINT_HEX((uint8_t *) param->get_value(), param->size());

            NotificationBus::get().notify_parameter_changed(this, param);
            _notify_clients(client_id, packet.header->type, param->get_value(), param->size());
            return Response::ok();
        }

        D_PRINTF("WebSocket: Unable to update parameter for type %s\r\n", __debug_enum_str(packet.header->type));
        return Response::code(ResponseCode::BAD_REQUEST);
    }

    D_PRINTF("WebSocket: Unsupported packet type %s\r\n", __debug_enum_str(packet.header->type));
    return Response::code(ResponseCode::BAD_COMMAND);
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::register_command(PacketEnumT type, Command command) {
    register_command(type, [cmd = std::move(command)](auto, auto) {
        cmd();
    });
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::register_command(PacketEnumT type, WebSocketCommand command) {
    _commands[type] = std::move(command);
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::register_data_request(PacketEnumT type, const AbstractParameter *parameter) {
    _data_requests[type] = parameter;
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::register_notification(PacketEnumT type, const AbstractParameter *parameter) {
    _notifications[type] = parameter;
    _parameters_packet_type[parameter] = type;
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::register_parameter(PacketEnumT type, AbstractParameter *parameter) {
    _parameters[type] = parameter;
    _parameters_packet_type[parameter] = type;
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::on_event(
    AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            _client_count += 1;
            D_PRINTF("WebSocket: client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
            break;

        case WS_EVT_DISCONNECT:
            if (_client_count > 0) {
                _client_count -= 1;
            } else {
                D_PRINT("WebSocket: Unexpected client disconnect.");
            }

            D_PRINTF("WebSocket: client #%u disconnected\r\n", client->id());
            break;

        case WS_EVT_DATA: {
            D_PRINTF("WebSocket: received packet, size: %u\r\n", len);

            if (len == 0) {
                send_response(client->id(), ~(uint16_t) 0, Response::code(ResponseCode::PACKET_LENGTH_EXCEEDED));
                return;
            }

            if (len > WS_MAX_PACKET_SIZE) {
                D_PRINTF("WebSocket: packet dropped. Max packet size %ui, but received %u\r\n", WS_MAX_PACKET_SIZE, len);
                send_response(client->id(), ~(uint16_t) 0, Response::code(ResponseCode::PACKET_LENGTH_EXCEEDED));
                return;
            }

            if (!_request_queue.can_acquire()) {
                D_PRINT("WebSocket: packet dropped. Queue is full");
                send_response(client->id(), ~(uint16_t) 0, Response::code(ResponseCode::TOO_MANY_REQUEST));
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

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::send_response(uint32_t client_id, uint16_t request_id, const Response &response) {
    auto header = PacketHeader<SystemPacketTypeEnum>{
        .signature = PACKET_SIGNATURE,
        .request_id = request_id
    };

    const void *data;

    switch (response.type) {
        case ResponseType::CODE:
            header.type = SystemPacketTypeEnum::RESPONSE_STRING;
            data = (void *) response.code_string();
            header.size = strlen((const char *) data);
            break;

        case ResponseType::STRING:
            header.type = SystemPacketTypeEnum::RESPONSE_STRING;
            header.size = strlen(response.body.str);
            data = (void *) response.body.str;
            break;

        case ResponseType::BINARY:
            if (response.body.buffer.size > WS_MAX_PACKET_BODY_SIZE) {
                D_PRINTF("WebSocket: response size too long: %u\r\n", response.body.buffer.size);
                return send_response(client_id, request_id, Response::code(ResponseCode::INTERNAL_ERROR));
            }

            header.type = SystemPacketTypeEnum::RESPONSE_BINARY;
            header.size = response.body.buffer.size;
            data = (void *) response.body.buffer.data;
            break;

        default:
            D_PRINTF("WebSocket: unknown response type %u\r\n", response.type);
            return send_response(client_id, request_id, Response::code(ResponseCode::INTERNAL_ERROR));
    }


    uint8_t response_data[sizeof(header) + header.size];
    memcpy(response_data, &header, sizeof(header));
    memcpy(response_data + sizeof(header), data, header.size);

    _ws.binary(client_id, response_data, sizeof(response_data));
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::_notify_clients(uint32_t sender_id, PacketEnumT type) {
    _notify_clients(sender_id, type, nullptr, 0);
}

template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::_notify_clients(uint32_t sender_id, PacketEnumT type, const void *data, uint8_t size) {
    if (_client_count == 0) return;

    uint8_t message[sizeof(PacketHeader<PacketEnumT>) + size];

    (*(PacketHeader<PacketEnumT> *) message) = PacketHeader<PacketEnumT>{PACKET_SIGNATURE, 0, type, size};
    mempcpy(message + sizeof(PacketHeader<PacketEnumT>), data, size);

    bool sent = false;
    for (auto &client: _ws.getClients()) {
        if (sender_id == client.id()) continue;

        VERBOSE(D_PRINTF("Websocket: send notification to client: %u\r\n", sender_id));
        _ws.binary(client.id(), message, sizeof(message));
        sent = true;
    }

    if (sent) {
        D_PRINTF("WebSocket: send notification: %s, total size: %u, data size: %u\r\n", __debug_enum_str(type), sizeof(message), size);
    }
}

template<typename PacketEnumT>
template<typename T>
void WebSocketServer<PacketEnumT>::_notify_clients(uint32_t sender_id, PacketEnumT type, const T &value) {
    _notify_clients(sender_id, type, &value, sizeof(value));
}


template<typename PacketEnumT>
void WebSocketServer<PacketEnumT>::_process_notification(void *sender, const AbstractParameter *parameter) {
    if (sender == this) return;

    auto it = _parameters_packet_type.find(parameter);
    if (it == _parameters_packet_type.end()) {
        VERBOSE(D_PRINT("WebSocket: Unsupported parameter notification"));
        return;
    }

    _notify_clients(-1, it->second, parameter->get_value(), parameter->size());
}
