#pragma once

#include "client/game_world_snapshot.hpp"

#include <cstdint>
#include <optional>

namespace tibia::client {

struct MapWindowCameraOffset {
    std::int32_t x{};
    std::int32_t y{};
};

struct MapWindowTilePosition {
    std::int32_t x{};
    std::int32_t y{};
};

[[nodiscard]] std::optional<MapWindowCameraOffset> resolve_map_window_camera_offset(
    const GameWorldSnapshot& snapshot) noexcept;

[[nodiscard]] std::optional<MapWindowTilePosition> project_map_window_tile_position(
    std::int32_t tile_x,
    std::int32_t tile_y,
    MapWindowCameraOffset camera_offset) noexcept;

}
