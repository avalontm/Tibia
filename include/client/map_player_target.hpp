#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_layout.hpp"
#include "client/map_window_camera.hpp"
#include "resources/tibia_dat.hpp"

#include <cstdint>
#include <optional>

namespace tibia::client {

[[nodiscard]] std::optional<std::uint32_t> map_creature_at(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const MapWindowLayout& viewport,
    MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y);

}
