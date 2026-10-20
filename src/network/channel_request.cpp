#include "network/channel_request.hpp"

#include "network/packet_stream.hpp"

#include <utility>

namespace tibia::network {

std::vector<std::uint8_t> encode_state6_channel_list_request() {
    PacketWriter writer;
    writer.write_u8(0x97);
    return std::move(writer).take();
}

std::vector<std::uint8_t> encode_state6_join_channel_request(
    std::uint16_t channel_id) {
    PacketWriter writer;
    writer.write_u8(0x98);
    writer.write_u16(channel_id);
    return std::move(writer).take();
}

std::vector<std::uint8_t> encode_state6_private_channel_request(
    std::string_view character_name) {
    if (character_name.empty()) return {};
    PacketWriter writer;
    writer.write_u8(0x9A);
    writer.write_string(character_name);
    return std::move(writer).take();
}

std::vector<std::uint8_t> encode_state6_close_channel_request(
    std::uint16_t channel_id) {
    PacketWriter writer;
    writer.write_u8(0x99);
    writer.write_u16(channel_id);
    return std::move(writer).take();
}

std::vector<std::uint8_t> encode_state6_close_private_channel_request(
    std::string_view character_name) {
    if (character_name.empty()) return {};
    PacketWriter writer;
    writer.write_u8(0x9C);
    writer.write_string(character_name);
    return std::move(writer).take();
}

std::vector<std::uint8_t> encode_state6_close_rule_violation_request() {
    PacketWriter writer;
    writer.write_u8(0x9D);
    return std::move(writer).take();
}

}
