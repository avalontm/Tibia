#pragma once

#include <array>
#include <cstdint>

namespace tibia::network {

[[nodiscard]] std::array<std::uint8_t, 2> encode_container_close_request(
    std::uint8_t container_id) noexcept;

[[nodiscard]] std::array<std::uint8_t, 2> encode_container_higher_request(
    std::uint8_t container_id) noexcept;

[[nodiscard]] std::array<std::uint8_t, 10> encode_container_use_item_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type,
    std::uint8_t action_parameter = 0) noexcept;

[[nodiscard]] std::array<std::uint8_t, 9> encode_container_look_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type) noexcept;

[[nodiscard]] std::array<std::uint8_t, 9> encode_container_rotate_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type) noexcept;

[[nodiscard]] std::array<std::uint8_t, 15> encode_container_move_up_request(
    std::uint8_t container_id,
    std::uint8_t slot,
    std::uint16_t item_type,
    std::uint8_t amount) noexcept;

}
