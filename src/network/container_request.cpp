#include "network/container_request.hpp"
#include "network/item_action_request.hpp"

namespace tibia::network {

std::array<std::uint8_t, 2> encode_container_close_request(
    std::uint8_t container_id) noexcept {
    return {0x87, container_id};
}

std::array<std::uint8_t, 2> encode_container_higher_request(
    std::uint8_t container_id) noexcept {
    return {0x88, container_id};
}

std::array<std::uint8_t, 10> encode_container_use_item_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type,
    std::uint8_t action_parameter) noexcept {
    const auto container_position_y = static_cast<std::uint16_t>(container_id + 0x40U);
    return {
        0x82,
        0xff,
        0xff,
        static_cast<std::uint8_t>(container_position_y),
        static_cast<std::uint8_t>(container_position_y >> 8U),
        slot,
        static_cast<std::uint8_t>(item_type),
        static_cast<std::uint8_t>(item_type >> 8U),
        slot,
        action_parameter
    };
}

std::array<std::uint8_t, 9> encode_container_look_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type) noexcept {
    const auto container_position_y = static_cast<std::uint16_t>(container_id + 0x40U);
    return {
        0x8c,
        0xff,
        0xff,
        static_cast<std::uint8_t>(container_position_y),
        static_cast<std::uint8_t>(container_position_y >> 8U),
        slot,
        static_cast<std::uint8_t>(item_type),
        static_cast<std::uint8_t>(item_type >> 8U),
        slot
    };
}

std::array<std::uint8_t, 9> encode_container_rotate_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type) noexcept {
    const auto container_position_y = static_cast<std::uint16_t>(container_id + 0x40U);
    return {
        0x85,
        0xff,
        0xff,
        static_cast<std::uint8_t>(container_position_y),
        static_cast<std::uint8_t>(container_position_y >> 8U),
        slot,
        static_cast<std::uint8_t>(item_type),
        static_cast<std::uint8_t>(item_type >> 8U),
        slot
    };
}

std::array<std::uint8_t, 15> encode_container_move_up_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type,
    std::uint8_t amount) noexcept {
    const auto container_position_y = static_cast<std::uint16_t>(container_id + 0x40U);
    return encode_move_item_request(
        ItemPosition{0xffff, container_position_y, slot, item_type, slot},
        0xffff, container_position_y, 0xfe, amount);
}

}
