#include "client/map_player_target.hpp"

#include "map/model.hpp"
#include "map/render_cell_filter.hpp"
#include "map/tile_object_order.hpp"
#include "render/creature_projection.hpp"

#include <cstdint>

namespace tibia::client {

std::optional<std::uint32_t> map_creature_at(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const MapWindowLayout& viewport,
    MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y) {
    if (!snapshot.map || viewport.width <= 0 || viewport.height <= 0 ||
        click_x < viewport.x || click_y < viewport.y ||
        click_x >= viewport.x + viewport.width ||
        click_y >= viewport.y + viewport.height)
        return std::nullopt;

    const auto relative_x = click_x - viewport.x;
    const auto relative_y = click_y - viewport.y;
    const auto logical_x = relative_x +
        (map_window_reference_width - viewport.width) / 2;
    const auto logical_y = relative_y +
        (map_window_reference_height - viewport.height) / 2;
    const auto highest_active_floor =
        map::compute_highest_active_render_floor(*snapshot.map, dat);
    if (!highest_active_floor) return std::nullopt;

    for (const auto& creature : snapshot.creatures) {
        const auto id = creature.state.creature_id;
        if (!creature.viewport_map_position)
            continue;
        const auto movement = render::resolve_creature_movement_offset(
            creature.state, snapshot.map->local_player_id,
            snapshot.local_player_movement);
        if (!movement) continue;
        const auto& position = *creature.viewport_map_position;
        bool occluded = false;
        for (const auto& tile : snapshot.map->tiles) {
            const auto tile_position = map::viewport_map_position(*snapshot.map, tile);
            if (!tile_position || tile_position->x != position.x ||
                tile_position->y != position.y || tile_position->floor <= position.floor ||
                tile_position->floor > *highest_active_floor)
                continue;
            const auto objects = map::resolve_native_tile_object_order(tile, dat);
            if (!objects || objects->empty()) continue;
            const auto& top = *objects->front();
            if (top.creature_id != 0 || top.type == 0 || top.type == 0x63) continue;
            const auto* item = dat.find_item(top.type);
            if (item != nullptr && item->has_native_property(0)) {
                occluded = true;
                break;
            }
        }
        if (occluded) continue;
        const auto left = static_cast<std::int64_t>(position.x) * map::map_tile_pixels +
            movement->x - camera_offset.x;
        const auto top = static_cast<std::int64_t>(position.y) * map::map_tile_pixels +
            movement->y - camera_offset.y;
        const auto center_x = left + map::map_tile_pixels / 2;
        const bool hit = no_stretching
            ? logical_x >= center_x - map::map_tile_pixels / 2 &&
                logical_x <= center_x + map::map_tile_pixels / 2 &&
                logical_y >= top && logical_y <= top + map::map_tile_pixels
            : [&] {
                const auto scaled_center_x = center_x * viewport.width /
                    map_window_reference_width;
                const auto scaled_top = top * viewport.height /
                    map_window_reference_height;
                const auto hit_half_width = (viewport.width / 15) / 2;
                const auto hit_height = viewport.height / 11;
                return relative_x >= scaled_center_x - hit_half_width &&
                    relative_x <= scaled_center_x + hit_half_width &&
                    relative_y >= scaled_top &&
                    relative_y <= scaled_top + hit_height;
            }();
        if (hit)
            return id;
    }
    return std::nullopt;
}

}
