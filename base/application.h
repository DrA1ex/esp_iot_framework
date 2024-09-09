#pragma once

#include <cstdint>
#include <map>
#include <type_traits>

#include "Arduino.h"

#include "../debug.h"
#include "../misc/event_topic.h"

template<typename PropE, typename PacketE = PropE,
        typename = std::enable_if_t<std::is_enum_v<PropE> && std::is_enum_v<PacketE>>>
struct PropertyMetadata {
    using PropEnumT = PropE;
    using PacketEnumT = PacketE;

    PropEnumT property;
    PacketEnumT packet_type;

    uint8_t value_offset = 0;
    uint8_t value_size = 0;

    const char *mqtt_in_topic = nullptr;
    const char *mqtt_out_topic = nullptr;
};

template<typename CfgT, typename MetaT,
        typename = std::enable_if_t<std::is_standard_layout_v<CfgT>>>
class AbstractApplication {
    EventTopic<typename MetaT::PropEnumT> _event_property_changed;

public:
    using ConfigT = CfgT;
    using MetaPropT = MetaT;
    using PropEnumT = typename MetaPropT::PropEnumT;
    using PacketEnumT = typename MetaPropT::PacketEnumT;

    virtual inline ConfigT &config() = 0;


    explicit AbstractApplication(const std::map<PacketEnumT, MetaT> &props);
    virtual ~AbstractApplication() = default;


    virtual inline const std::map<PacketEnumT, MetaPropT> &packet_meta() { return _packetTypeMetadataMap; }
    virtual inline const std::map<PropEnumT, std::vector<MetaPropT>> &property_meta() { return _propertyMetadataMap; }
    virtual inline const std::map<String, MetaPropT> &topic_property_meta() { return _topicPropertyMetadata; }

    virtual inline EventTopic<PropEnumT> &event_property_changed() { return _event_property_changed; }
    virtual inline void notify_property_changed(void *sender, PropEnumT param, void *arg) {
        event_property_changed().publish(sender, param, arg);
    }

private:
    const std::map<PacketEnumT, MetaT> &_packetTypeMetadataMap;
    const std::map<PropEnumT, std::vector<MetaT>> _propertyMetadataMap;
    const std::map<String, MetaT> _topicPropertyMetadata;


    std::map<PropEnumT, std::vector<MetaT>> _build_property_metadata_map(const std::map<PacketEnumT, MetaT> &packetMapping);
    std::map<String, MetaT> _build_topic_property_metadata_map(const std::map<PacketEnumT, MetaT> &packetMapping);
};

template<typename CfgT, typename MetaT, typename S1>
AbstractApplication<CfgT, MetaT, S1>::AbstractApplication(const std::map<PacketEnumT, MetaT> &props):
        _packetTypeMetadataMap(props),
        _propertyMetadataMap(_build_property_metadata_map(_packetTypeMetadataMap)),
        _topicPropertyMetadata(_build_topic_property_metadata_map(_packetTypeMetadataMap)) {
    VERBOSE(
            D_PRINT("Application config properties:");
            for (auto &[key, meta]: _packetTypeMetadataMap) {
                D_PRINTF("Key: %s\r\n", __debug_enum_str(key));
                D_PRINTF("---- Property: %s\r\n", __debug_enum_str(meta.property));
                D_PRINTF("---- Packet: %s\r\n", __debug_enum_str(meta.packet_type));
                D_PRINTF("---- Topic-in: %s\r\n", meta.mqtt_in_topic ?: "null");
                D_PRINTF("---- Topic-out: %s\r\n", meta.mqtt_out_topic ?: "null");
                D_PRINTF("---- Offset: %u\r\n", meta.value_offset);
                D_PRINTF("---- Size: %u\r\n", meta.value_size);
            }
    )
}


template<typename CfgT, typename MetaT, typename S1>
std::map<String, MetaT>
AbstractApplication<CfgT, MetaT, S1>::_build_topic_property_metadata_map(const std::map<PacketEnumT, MetaT> &packetMapping) {
    std::map<String, MetaT> result;

    for (const auto &[packetType, metadata]: packetMapping) {
        if (metadata.mqtt_in_topic == nullptr) continue;

        result[metadata.mqtt_in_topic] = metadata;
    }

    return result;
}

template<typename CfgT, typename MetaT, typename S1>
std::map<typename MetaT::PropEnumT, std::vector<MetaT>>
AbstractApplication<CfgT, MetaT, S1>::_build_property_metadata_map(const std::map<PacketEnumT, MetaT> &packetMapping) {
    std::map<PropEnumT, std::vector<MetaT>> result;

    for (auto &[packetType, metadata]: packetMapping) {
        std::vector<MetaT> &prop = result[metadata.property];
        prop.push_back(metadata);
    }

    return result;
}