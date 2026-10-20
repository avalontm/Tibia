#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_camera.hpp"
#include "client/map_window_layout.hpp"
#include "map/model.hpp"
#include "resources/tibia_dat.hpp"

#include <cstdint>
#include <optional>

namespace tibia::map {

struct MapPosition {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint8_t floor{};
};

[[nodiscard]] std::optional<MapPosition> map_position_at(
    const client::GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const client::MapWindowLayout& viewport,
    client::MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y);

}
