#include "map/use_target_at.hpp"

#include "map/map_position_at.hpp"
#include "map/tile_object_order.hpp"
#include "map/use_target_selection.hpp"

namespace tibia::map {

std::optional<UseTarget> use_target_at(
    const client::GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const client::MapWindowLayout& viewport,
    client::MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y) {
    const auto position = map_position_at(snapshot, dat, viewport, camera_offset,
        no_stretching, click_x, click_y);
    if (!position || !snapshot.map) return std::nullopt;

    const Tile* selected_tile{};
    for (const auto& tile : snapshot.map->tiles) {
        if (tile.x != position->x || tile.y != position->y ||
            tile.floor != position->floor)
            continue;
        if (selected_tile) return std::nullopt;
        selected_tile = &tile;
    }
    if (!selected_tile) return std::nullopt;

    const auto native_objects = resolve_native_tile_object_order(*selected_tile, dat);
    if (!native_objects) return std::nullopt;
    const auto stack_position =
        select_native_use_target_stack_position(*native_objects, dat);
    if (!stack_position || *stack_position >= native_objects->size())
        return std::nullopt;
    const auto& object = *(*native_objects)[*stack_position];
    if (object.type == 0) return std::nullopt;
    return UseTarget{selected_tile, &object, *stack_position};
}

}
