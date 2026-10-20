#include "client/map_window_camera.hpp"

#include "render/creature_projection.hpp"

#include <bit>
#include <limits>

namespace tibia::client {
namespace {

std::int32_t add_native_offset(std::int32_t value, std::int32_t adjustment) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(value) + static_cast<std::uint32_t>(adjustment));
}

}

std::optional<MapWindowCameraOffset> resolve_map_window_camera_offset(
    const GameWorldSnapshot& snapshot) noexcept {
    if (!snapshot.map || snapshot.map->local_player_id == 0) return std::nullopt;
    for (const auto& creature : snapshot.creatures) {
        if (creature.state.creature_id != snapshot.map->local_player_id) continue;
        const auto movement = render::resolve_creature_movement_offset(
            creature.state, snapshot.map->local_player_id, snapshot.local_player_movement);
        if (!movement) return std::nullopt;
        return MapWindowCameraOffset{
            add_native_offset(movement->x, 32),
            add_native_offset(movement->y, 32)
        };
    }
    return std::nullopt;
}

std::optional<MapWindowTilePosition> project_map_window_tile_position(
    std::int32_t tile_x,
    std::int32_t tile_y,
    MapWindowCameraOffset camera_offset) noexcept {
    const auto x = static_cast<std::int64_t>(tile_x) * 32 - camera_offset.x;
    const auto y = static_cast<std::int64_t>(tile_y) * 32 - camera_offset.y;
    if (x < std::numeric_limits<std::int32_t>::min() ||
        x > std::numeric_limits<std::int32_t>::max() ||
        y < std::numeric_limits<std::int32_t>::min() ||
        y > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    return MapWindowTilePosition{
        static_cast<std::int32_t>(x),
        static_cast<std::int32_t>(y)
    };
}

}
