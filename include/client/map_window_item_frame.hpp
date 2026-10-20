#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_camera.hpp"
#include "map/light_field.hpp"
#include "map/window_tile_cache.hpp"
#include "render/map_item_router.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tibia::client {

enum class MapWindowItemFrameStatus : std::uint8_t {
    complete,
    no_map,
    unresolved_filter_grid,
    unresolved_active_floor,
    unresolved_map_item,
    unresolved_cell_data,
    unresolved_effect_state
};

struct MapWindowItemVisitFrame {
    map::MapItemRenderVisit visit;
    render::MapItemVisitRouteBatch standard_route;
    render::MapItemVisitRouteBatch deferred_route;
};

struct MapWindowItemFrame {
    MapWindowItemFrameStatus status{MapWindowItemFrameStatus::no_map};
    std::uint32_t animation_clock{};
    std::int32_t highest_active_floor{};
    std::int32_t camera_offset_x{};
    std::int32_t camera_offset_y{};
    std::int32_t ring_offset_x{};
    std::int32_t ring_offset_y{};
    std::size_t deferred_objects{};
    std::array<map::LightCellMask, map::light_field_floor_count> floor_light_property_masks{};
    std::array<map::LightCellMask, map::light_field_floor_count> floor_light_property_18_masks{};
    std::vector<map::LightSource> light_sources;
    std::vector<MapWindowItemVisitFrame> visits;
    std::vector<map::MapTileRenderBatch> visible_tiles;
};

[[nodiscard]] MapWindowItemFrame prepare_map_window_item_frame(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    MapWindowCameraOffset camera_offset,
    map::MapWindowTileCache& tile_cache);

}
