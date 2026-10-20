#pragma once

#include "map/model.hpp"
#include "map/floor_window.hpp"

#include <cstdint>

namespace tibia::resources { struct TibiaDatFile; }

namespace tibia::map {

enum class MovementTileState : std::uint8_t {
    clear,
    blocked,
    invalid_coordinate,
    unresolved
};

[[nodiscard]] MovementTileState query_movement_tile(
    const DecodedMap& map, const tibia::resources::TibiaDatFile& dat,
    std::int32_t local_x, std::int32_t local_y, std::int32_t local_floor) noexcept;
[[nodiscard]] MovementTileState query_movement_tile(
    const FloorWindow::Cell& cell, const tibia::resources::TibiaDatFile& dat) noexcept;

}
