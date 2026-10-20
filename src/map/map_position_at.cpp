#include "map/map_position_at.hpp"

#include "map/render_cell_filter.hpp"
#include "map/render_queue.hpp"
#include "map/tile_object_order.hpp"

#include <algorithm>

namespace tibia::map {

std::optional<MapPosition> map_position_at(
    const client::GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const client::MapWindowLayout& viewport,
    client::MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y) {
    if (!snapshot.map || viewport.width <= 0 || viewport.height <= 0 ||
        click_x < viewport.x || click_y < viewport.y ||
        click_x >= viewport.x + viewport.width ||
        click_y >= viewport.y + viewport.height)
        return std::nullopt;

    const auto pixel_x = click_x - viewport.x;
    const auto pixel_y = click_y - viewport.y;
    const auto logical_x = no_stretching
        ? pixel_x + (client::map_window_reference_width - viewport.width) / 2
        : static_cast<std::int32_t>(static_cast<std::int64_t>(pixel_x) *
            client::map_window_reference_width / viewport.width);
    const auto logical_y = no_stretching
        ? pixel_y + (client::map_window_reference_height - viewport.height) / 2
        : static_cast<std::int32_t>(static_cast<std::int64_t>(pixel_y) *
            client::map_window_reference_height / viewport.height);
    const auto local_x = (logical_x + camera_offset.x) / map_tile_pixels;
    const auto local_y = (logical_y + camera_offset.y) / map_tile_pixels;
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y)
        return std::nullopt;

    const auto maximum_floor = compute_highest_active_render_floor(*snapshot.map, dat);
    if (!maximum_floor) return std::nullopt;
    const auto is_property = [&dat](const Object& object, std::uint8_t property) {
        if (object.creature_id != 0 || object.type == 0 || object.type == 0x63)
            return false;
        const auto* item = dat.find_item(object.type);
        return item && item->has_native_property(property);
    };
    const Tile* selected{};
    auto selected_local_floor = 0;
    for (auto floor = *maximum_floor; floor > 0; --floor) {
        const Tile* tile_at_floor{};
        for (const auto& tile : snapshot.map->tiles) {
            const auto position = viewport_map_position(*snapshot.map, tile);
            if (!position || position->x != local_x || position->y != local_y ||
                position->floor != floor)
                continue;
            if (tile_at_floor) return std::nullopt;
            tile_at_floor = &tile;
        }
        if (!tile_at_floor) continue;
        const auto objects = resolve_native_tile_object_order(*tile_at_floor, dat);
        if (!objects) return std::nullopt;
        if (objects->empty()) continue;
        const auto& first = *objects->front();
        if (is_property(first, 0) || is_property(first, 2)) {
            selected = tile_at_floor;
            selected_local_floor = floor;
        } else if (objects->size() > 1 && is_property(*(*objects)[1], 2)) {
            selected = tile_at_floor;
            selected_local_floor = floor;
        }
        if (selected) break;
    }
    if (selected)
        return MapPosition{selected->x, selected->y, selected->floor};

    const auto world_floor = snapshot.map->center_floor < 8
        ? 7 - selected_local_floor
        : static_cast<int>(snapshot.map->center_floor) + 2 - selected_local_floor;
    const auto floor_delta = static_cast<std::int32_t>(snapshot.map->center_floor) -
        world_floor;
    const auto world_x = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(snapshot.map->center_x) + local_x - 8 + floor_delta);
    const auto world_y = static_cast<std::uint16_t>(
        static_cast<std::int32_t>(snapshot.map->center_y) + local_y - 6 + floor_delta);
    return MapPosition{world_x, world_y, static_cast<std::uint8_t>(world_floor)};
}

}
