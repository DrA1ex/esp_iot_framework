#pragma once

#include "parameter.h"

template<typename PacketEnumT>
struct BinaryProtocolMeta {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");

    std::optional<PacketEnumT> packet_type = {};
};

struct MqttProtocolMeta {
    const char *topic_in = nullptr;
    const char *topic_out = nullptr;
};

struct AbstractPropertyMeta {
    [[nodiscard]] virtual AbstractParameter *get_parameter() = 0;
    [[nodiscard]] virtual void *get_binary_protocol() = 0;
    [[nodiscard]] virtual MqttProtocolMeta *get_mqtt_protocol() = 0;

    operator AbstractParameter *() { return get_parameter(); } // NOLINT(*-explicit-constructor)

    virtual ~AbstractPropertyMeta() = default;
};

template<typename PacketEnumT, typename ParameterT>
struct PropertyMeta : AbstractPropertyMeta {
    static_assert(std::is_enum_v<PacketEnumT>, "PacketEnumT should be an enum");
    static_assert(std::is_base_of_v<AbstractParameter, ParameterT>, "ParameterT should be derived from AbstractParameter");

    ParameterT parameter;

    [[nodiscard]] AbstractParameter *get_parameter() final { return &parameter; }
    [[nodiscard]] void *get_binary_protocol() final { return &binary_protocol; }
    [[nodiscard]] MqttProtocolMeta *get_mqtt_protocol() final { return &mqtt_protocol; }

    BinaryProtocolMeta<PacketEnumT> binary_protocol;
    MqttProtocolMeta mqtt_protocol;

    PropertyMeta() = default;

    PropertyMeta(ParameterT &&parameter) : parameter(std::move(parameter)) {} // NOLINT(*-explicit-constructor)

    PropertyMeta(PacketEnumT packet_type, ParameterT &&parameter)
        : parameter(std::move(parameter)), binary_protocol{packet_type} {}

    PropertyMeta(const char *topic_in, const char *topic_out, ParameterT &&parameter)
        : parameter(std::move(parameter)), mqtt_protocol{topic_in, topic_out} {}
    PropertyMeta(const char *topic_out, ParameterT &&parameter)
        : parameter(std::move(parameter)), mqtt_protocol{nullptr, topic_out} {}

    PropertyMeta(PacketEnumT packet_type, const char *topic_in, const char *topic_out, ParameterT &&parameter)
        : parameter(std::move(parameter)), binary_protocol{packet_type}, mqtt_protocol{topic_in, topic_out} {}
    PropertyMeta(PacketEnumT packet_type, const char *topic_out, ParameterT &&parameter)
        : parameter(std::move(parameter)), binary_protocol{packet_type}, mqtt_protocol{nullptr, topic_out} {}
};
