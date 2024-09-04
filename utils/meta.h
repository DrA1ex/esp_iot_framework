#pragma once

#include <cstdint>
#include <utility>

#include "../base/application.h"

template<typename T, typename M>
constexpr std::size_t __offset_of(M T::* member) {
    return reinterpret_cast<std::size_t>(&(((T *) nullptr)->*member));
}

template<typename PropEnumT, typename PacketEnumT,
        typename = std::enable_if_t<std::is_enum_v<PropEnumT> && std::is_enum_v<PacketEnumT>>>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> __build_entry(
        PropEnumT prop, PacketEnumT packet, uint8_t offset, uint8_t size, const char *mqtt_in_topic, const char *mqtt_out_topic) {
    return std::make_pair(
            packet, PropertyMetadata<PropEnumT, PacketEnumT>{
                    .property= prop, .packet_type=packet,
                    .value_offset=  offset, .value_size= size,
                    .mqtt_in_topic = mqtt_in_topic, .mqtt_out_topic= mqtt_out_topic
            }
    );
}

template<typename CfgT, typename R1, typename PropEnumT, typename PacketEnumT>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> __meta_entry_m(
        uint8_t offset, PropEnumT prop, PacketEnumT packet, const char *mqtt_in_topic, const char *mqtt_out_topic,
        R1 CfgT::* member1) {
    return __build_entry(prop, packet, offset + __offset_of(member1), sizeof(R1), mqtt_in_topic, mqtt_out_topic);
}

template<typename CfgT, typename R1, typename R2, typename PropEnumT, typename PacketEnumT>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> __meta_entry_m(
        uint8_t offset, PropEnumT prop, PacketEnumT packet, const char *mqtt_in_topic, const char *mqtt_out_topic,
        R1 CfgT::* member1, R2 R1::* member2) {
    return __meta_entry_m(offset + __offset_of(member1), prop, packet, mqtt_in_topic, mqtt_out_topic, member2);
}

template<typename CfgT, typename R1, typename R2, typename PropEnumT, typename PacketEnumT, typename... Members>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> __meta_entry_m(
        uint8_t offset, PropEnumT prop, PacketEnumT packet, const char *mqtt_in_topic, const char *mqtt_out_topic,
        R1 CfgT::* member1, R2 R1::* member2, Members... members) {
    return __meta_entry_m(offset + __offset_of(member1), prop, packet, mqtt_in_topic, mqtt_out_topic, member2, members...);
}

template<typename PropEnumT, typename... Members,
        typename = std::enable_if_t<std::is_enum_v<PropEnumT>>>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT>> define_meta_entry(
        PropEnumT prop, Members... members) {
    return __meta_entry_m(0, prop, prop, nullptr, nullptr, members...);
}

template<typename PropEnumT, typename... Members,
        typename = std::enable_if_t<std::is_enum_v<PropEnumT>>>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT>> define_meta_entry(
        PropEnumT prop, const char *mqtt_in_topic, const char *mqtt_out_topic, Members... members) {
    return __meta_entry_m(0, prop, prop, mqtt_in_topic, mqtt_out_topic, members...);
}

template<typename PropEnumT, typename PacketEnumT, typename... Members,
        typename = std::enable_if_t<std::is_enum_v<PropEnumT> && std::is_enum_v<PacketEnumT>>>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> define_meta_entry(
        PropEnumT prop, PacketEnumT packet, Members... members) {
    return __meta_entry_m(0, prop, packet, nullptr, nullptr, members...);
}

template<typename PropEnumT, typename PacketEnumT, typename... Members,
        typename = std::enable_if_t<std::is_enum_v<PropEnumT> && std::is_enum_v<PacketEnumT>>>
constexpr std::pair<PacketTypeEnum, PropertyMetadata<PropEnumT, PacketEnumT>> define_meta_entry(
        PropEnumT prop, PacketEnumT packet, const char *mqtt_in_topic, const char *mqtt_out_topic, Members... members) {
    return __meta_entry_m(0, prop, packet, mqtt_in_topic, mqtt_out_topic, members...);
}
