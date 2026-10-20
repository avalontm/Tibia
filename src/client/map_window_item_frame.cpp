#include "client/map_window_item_frame.hpp"
#include "client/map_window_light_sources.hpp"

#include "map/item_render_traversal.hpp"
#include "map/render_queue.hpp"
#include "map/render_cell_filter.hpp"

#include <utility>

namespace tibia::client {

MapWindowItemFrame prepare_map_window_item_frame(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    MapWindowCameraOffset camera_offset,
    map::MapWindowTileCache& tile_cache) {
    MapWindowItemFrame frame;
    frame.animation_clock = animation_clock;
    if (!snapshot.map) return frame;
    if (!snapshot.render_filter_grid) {
        frame.status = MapWindowItemFrameStatus::unresolved_filter_grid;
        return frame;
    }
    frame.camera_offset_x = camera_offset.x;
    frame.camera_offset_y = camera_offset.y;
    frame.ring_offset_x = snapshot.map->center_x % map::map_cache_columns;
    frame.ring_offset_y = snapshot.map->center_y % map::map_cache_rows;
    const auto highest_active_floor = map::compute_highest_active_render_floor(
        *snapshot.map, dat);
    if (!highest_active_floor) {
        frame.status = MapWindowItemFrameStatus::unresolved_active_floor;
        return frame;
    }

    frame.highest_active_floor = *highest_active_floor;
    tile_cache.begin_frame();
    const auto cell_provider = tile_cache.make_provider(*snapshot.map);
    const auto cell_lookup = map::make_filtered_tile_render_cell_lookup(
        *snapshot.render_filter_grid, cell_provider);
    frame.status = MapWindowItemFrameStatus::complete;
    if (snapshot.has_unresolved_projectile_effects)
        frame.status = MapWindowItemFrameStatus::unresolved_effect_state;

    for (std::int32_t floor = 0; floor <= *highest_active_floor; ++floor) {
        for (std::int32_t local_x = 0; local_x < map::map_viewport_tiles_x; ++local_x) {
            for (std::int32_t local_y = 0; local_y < map::map_viewport_tiles_y; ++local_y) {
                const auto traversal = map::collect_map_item_render_visits(
                    *snapshot.map, dat, local_x, local_y,
                    static_cast<std::uint8_t>(floor),
                    local_x * map::map_tile_pixels - camera_offset.x,
                    local_y * map::map_tile_pixels - camera_offset.y);
                if (traversal.status != map::MapItemTraversalStatus::complete &&
                    frame.status == MapWindowItemFrameStatus::complete) {
                    frame.status = MapWindowItemFrameStatus::unresolved_map_item;
                }
                for (const auto& visit : traversal.visits) {
                    auto route = render::route_map_item_visit_sprite_objects(
                        visit, dat, animation_clock,
                        camera_offset.x, camera_offset.y, cell_lookup);
                    if (route.status == render::MapItemVisitRouteStatus::unresolved_cell_data) {
                        frame.status = MapWindowItemFrameStatus::unresolved_cell_data;
                    }
                    if (route.status == render::MapItemVisitRouteStatus::unresolved_render_inputs ||
                        route.status == render::MapItemVisitRouteStatus::unresolved_sprite_layout) {
                        if (frame.status == MapWindowItemFrameStatus::complete ||
                            frame.status == MapWindowItemFrameStatus::unresolved_effect_state)
                            frame.status = MapWindowItemFrameStatus::unresolved_map_item;
                    }
                    render::MapItemVisitRouteBatch deferred_route;
                    if (visit.deferred_object != nullptr) {
                        ++frame.deferred_objects;
                        auto effect_index = -1;
                        for (const auto& effect : snapshot.map_effects) {
                            if (effect.local_x == visit.local_x &&
                                effect.local_y == visit.local_y &&
                                effect.local_floor == visit.floor) {
                                effect_index = static_cast<std::int32_t>(effect.index);
                                break;
                            }
                        }
                        deferred_route = render::route_deferred_map_item_visit(
                            visit, effect_index, dat, animation_clock,
                            camera_offset.x, camera_offset.y, cell_lookup);
                    }
                    if (deferred_route.status == render::MapItemVisitRouteStatus::waiting_for_effects) {
                        if (frame.status == MapWindowItemFrameStatus::complete)
                            frame.status = MapWindowItemFrameStatus::unresolved_effect_state;
                    } else if (deferred_route.status == render::MapItemVisitRouteStatus::unresolved_cell_data) {
                        frame.status = MapWindowItemFrameStatus::unresolved_cell_data;
                    } else if (deferred_route.status == render::MapItemVisitRouteStatus::unresolved_render_inputs ||
                        deferred_route.status == render::MapItemVisitRouteStatus::unresolved_sprite_layout) {
                        if (frame.status == MapWindowItemFrameStatus::complete ||
                            frame.status == MapWindowItemFrameStatus::unresolved_effect_state)
                            frame.status = MapWindowItemFrameStatus::unresolved_map_item;
                    }
                    frame.visits.push_back({visit, std::move(route), std::move(deferred_route)});
                }
            }
        }
    }

    frame.visible_tiles = map::queue_visible_map_tiles(
        camera_offset.x, camera_offset.y, cell_provider);
    for (const auto& entry : frame.visits) {
        const auto& visit = entry.visit;
        if (visit.neighboring_object_pass || visit.tile == nullptr ||
            visit.floor >= map::light_field_floor_count || visit.ordered_objects.empty())
            continue;
        const auto& object = *visit.ordered_objects.front();
        const auto cell_index = static_cast<std::size_t>(visit.local_x) *
            map::light_field_height + static_cast<std::size_t>(visit.local_y);
        if (object.creature_id == 0 && object.type != 0 && object.type != 0x63) {
            const auto* item = dat.find_item(object.type);
            if (item != nullptr && item->has_native_property(0))
                frame.floor_light_property_masks[visit.floor][cell_index] = 1;
        }
        for (const auto* stacked_object : visit.ordered_objects) {
            if (stacked_object->creature_id != 0 || stacked_object->type == 0 ||
                stacked_object->type == 0x63)
                continue;
            const auto* item = dat.find_item(stacked_object->type);
            if (item != nullptr && item->has_native_property(0x18)) {
                frame.floor_light_property_18_masks[visit.floor][cell_index] = 1;
                break;
            }
        }
    }
    frame.light_sources = collect_map_window_item_light_sources(frame, dat);
    auto creature_light_sources = collect_map_window_creature_light_sources(
        snapshot, frame, dat);
    frame.light_sources.insert(frame.light_sources.end(),
        creature_light_sources.begin(), creature_light_sources.end());
    return frame;
}

}
