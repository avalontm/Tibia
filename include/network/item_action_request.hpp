#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tibia::network {

struct ItemPosition {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t z{};
    std::uint16_t item_type{};
    std::uint8_t stack_position{};
};

[[nodiscard]] std::array<std::uint8_t, 17> encode_use_with_item_request(
    const ItemPosition& source,
    const ItemPosition& target) noexcept;

[[nodiscard]] std::array<std::uint8_t, 9> encode_use_item_request(
    const ItemPosition& item,
    std::uint8_t action_parameter = 0) noexcept;

[[nodiscard]] std::array<std::uint8_t, 9> encode_look_item_request(
    const ItemPosition& item) noexcept;

[[nodiscard]] std::array<std::uint8_t, 9> encode_rotate_item_request(
    const ItemPosition& item) noexcept;

[[nodiscard]] std::array<std::uint8_t, 15> encode_move_item_request(
    const ItemPosition& source,
    std::uint16_t destination_x,
    std::uint16_t destination_y,
    std::uint8_t destination_z,
    std::uint8_t amount) noexcept;

[[nodiscard]] std::array<std::uint8_t, 13> encode_use_item_on_creature_request(
    const ItemPosition& source,
    std::uint32_t creature_id) noexcept;

[[nodiscard]] std::array<std::uint8_t, 13> encode_trade_with_creature_request(
    const ItemPosition& source,
    std::uint32_t creature_id) noexcept;

}
