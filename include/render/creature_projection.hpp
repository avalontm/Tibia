#pragma once

#include "map/render_queue.hpp"
#include "navigation/movement_state.hpp"
#include "network/message_dispatcher.hpp"

#include <cstdint>
#include <optional>

namespace tibia::render {

struct CreatureMovementOffset {
    std::int32_t x{};
    std::int32_t y{};
};

struct CreatureScreenPosition {
    std::int32_t x{};
    std::int32_t y{};
};

[[nodiscard]] std::optional<CreatureMovementOffset> resolve_creature_movement_offset(
    const network::State6CreatureRuntimeState& creature,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state) noexcept;

[[nodiscard]] std::optional<CreatureScreenPosition> project_creature_screen_position(
    std::int32_t tile_x, std::int32_t tile_y,
    std::int32_t reference_tile_x, std::int32_t reference_tile_y,
    CreatureMovementOffset movement_offset,
    std::int32_t outfit_extent_x, std::int32_t outfit_extent_y) noexcept;

}
