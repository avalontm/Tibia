#pragma once

#include "network/item_action_request.hpp"

#include <cstdint>
#include <optional>
#include <array>

namespace tibia::client {

struct MoveItemDestination {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t z{};
};

struct MoveItemDialogState {
    network::ItemPosition source;
    MoveItemDestination destination;
    std::uint8_t maximum_amount{};
    std::uint8_t amount{};
};

struct MoveItemDialogRequest {
    network::ItemPosition source;
    MoveItemDestination destination;
    std::uint8_t amount{};
};

[[nodiscard]] std::optional<MoveItemDialogState> make_move_item_dialog_state(
    const network::ItemPosition& source,
    MoveItemDestination destination,
    std::uint8_t source_count) noexcept;

[[nodiscard]] bool set_move_item_dialog_amount(
    MoveItemDialogState& state,
    std::uint8_t amount) noexcept;

[[nodiscard]] std::optional<MoveItemDialogRequest> resolve_move_item_dialog_request(
    const MoveItemDialogState& state) noexcept;

[[nodiscard]] std::optional<std::array<std::uint8_t, 15>> encode_move_item_dialog_request(
    const MoveItemDialogState& state) noexcept;

}
