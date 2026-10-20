#include "network/item_action_request.hpp"

namespace tibia::network {
namespace {

template<std::size_t Size>
void write_position(std::array<std::uint8_t, Size>& payload,
    std::size_t& offset, const ItemPosition& position) noexcept {
    payload[offset++] = static_cast<std::uint8_t>(position.x);
    payload[offset++] = static_cast<std::uint8_t>(position.x >> 8U);
    payload[offset++] = static_cast<std::uint8_t>(position.y);
    payload[offset++] = static_cast<std::uint8_t>(position.y >> 8U);
    payload[offset++] = position.z;
    payload[offset++] = static_cast<std::uint8_t>(position.item_type);
    payload[offset++] = static_cast<std::uint8_t>(position.item_type >> 8U);
    payload[offset++] = position.stack_position;
}

}

std::array<std::uint8_t, 17> encode_use_with_item_request(
    const ItemPosition& source,
    const ItemPosition& target) noexcept {
    std::array<std::uint8_t, 17> payload{};
    payload[0] = 0x83;
    std::size_t offset = 1;
    write_position(payload, offset, source);
    write_position(payload, offset, target);
    return payload;
}

std::array<std::uint8_t, 9> encode_use_item_request(
    const ItemPosition& item, std::uint8_t action_parameter) noexcept {
    std::array<std::uint8_t, 9> payload{};
    payload[0] = 0x82;
    std::size_t offset = 1;
    write_position(payload, offset, item);
    payload[offset] = action_parameter;
    return payload;
}

std::array<std::uint8_t, 9> encode_look_item_request(
    const ItemPosition& item) noexcept {
    std::array<std::uint8_t, 9> payload{};
    payload[0] = 0x8c;
    std::size_t offset = 1;
    write_position(payload, offset, item);
    return payload;
}

std::array<std::uint8_t, 9> encode_rotate_item_request(
    const ItemPosition& item) noexcept {
    std::array<std::uint8_t, 9> payload{};
    payload[0] = 0x85;
    std::size_t offset = 1;
    write_position(payload, offset, item);
    return payload;
}

std::array<std::uint8_t, 15> encode_move_item_request(
    const ItemPosition& source,
    std::uint16_t destination_x,
    std::uint16_t destination_y,
    std::uint8_t destination_z,
    std::uint8_t amount) noexcept {
    std::array<std::uint8_t, 15> payload{};
    payload[0] = 0x78;
    std::size_t offset = 1;
    write_position(payload, offset, source);
    payload[offset++] = static_cast<std::uint8_t>(destination_x);
    payload[offset++] = static_cast<std::uint8_t>(destination_x >> 8U);
    payload[offset++] = static_cast<std::uint8_t>(destination_y);
    payload[offset++] = static_cast<std::uint8_t>(destination_y >> 8U);
    payload[offset++] = destination_z;
    payload[offset] = amount;
    return payload;
}

std::array<std::uint8_t, 13> encode_use_item_on_creature_request(
    const ItemPosition& source,
    std::uint32_t creature_id) noexcept {
    std::array<std::uint8_t, 13> payload{};
    payload[0] = 0x84;
    std::size_t offset = 1;
    payload[offset++] = static_cast<std::uint8_t>(source.x);
    payload[offset++] = static_cast<std::uint8_t>(source.x >> 8U);
    payload[offset++] = static_cast<std::uint8_t>(source.y);
    payload[offset++] = static_cast<std::uint8_t>(source.y >> 8U);
    payload[offset++] = source.z;
    payload[offset++] = static_cast<std::uint8_t>(source.item_type);
    payload[offset++] = static_cast<std::uint8_t>(source.item_type >> 8U);
    payload[offset++] = source.stack_position;
    payload[offset++] = static_cast<std::uint8_t>(creature_id);
    payload[offset++] = static_cast<std::uint8_t>(creature_id >> 8U);
    payload[offset++] = static_cast<std::uint8_t>(creature_id >> 16U);
    payload[offset] = static_cast<std::uint8_t>(creature_id >> 24U);
    return payload;
}

std::array<std::uint8_t, 13> encode_trade_with_creature_request(
    const ItemPosition& source,
    std::uint32_t creature_id) noexcept {
    std::array<std::uint8_t, 13> payload{};
    payload[0] = 0x7d;
    std::size_t offset = 1;
    write_position(payload, offset, source);
    payload[offset++] = static_cast<std::uint8_t>(creature_id);
    payload[offset++] = static_cast<std::uint8_t>(creature_id >> 8U);
    payload[offset++] = static_cast<std::uint8_t>(creature_id >> 16U);
    payload[offset] = static_cast<std::uint8_t>(creature_id >> 24U);
    return payload;
}

}
