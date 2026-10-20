#include "map/collision.hpp"

#include "resources/tibia_dat.hpp"

#include <algorithm>

namespace tibia::map {

MovementTileState query_movement_tile(
    const FloorWindow::Cell& cell, const tibia::resources::TibiaDatFile& dat) noexcept {
    for (const auto& object : cell.objects) {
        if (object.creature_id != 0) continue;
        const auto* item = dat.find_item(object.type);
        if (item == nullptr) return MovementTileState::unresolved;
        if (item->has_native_property(0x0F)) return MovementTileState::blocked;
    }
    return MovementTileState::clear;
}

MovementTileState query_movement_tile(const DecodedMap& map,
    const tibia::resources::TibiaDatFile& dat, std::int32_t local_x,
    std::int32_t local_y, std::int32_t local_floor) noexcept {
    if (local_x < 0 || local_x >= static_cast<std::int32_t>(floor_window_width) ||
        local_y < 0 || local_y >= static_cast<std::int32_t>(floor_window_height) ||
        local_floor < 0 || local_floor >= static_cast<std::int32_t>(floor_window_depth))
        return MovementTileState::invalid_coordinate;
    const auto tile = std::find_if(map.tiles.begin(), map.tiles.end(), [&](const Tile& value) {
        const auto floor_delta = static_cast<std::int32_t>(map.center_floor) - value.floor;
        const auto x = static_cast<std::int32_t>(value.x) + 8 - map.center_x - floor_delta;
        const auto y = static_cast<std::int32_t>(value.y) + 6 - map.center_y - floor_delta;
        const auto floor = map.center_floor < 8
            ? 7 - static_cast<std::int32_t>(map.center_floor) + floor_delta
            : floor_delta + 2;
        return x == local_x && y == local_y && floor == local_floor;
    });
    if (tile == map.tiles.end()) return MovementTileState::clear;
    FloorWindow::Cell cell;
    cell.objects = tile->objects;
    return query_movement_tile(cell, dat);
}

}
