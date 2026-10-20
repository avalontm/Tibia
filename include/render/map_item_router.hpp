#pragma once

#include "map/item_render_traversal.hpp"
#include "render/map_item_layout.hpp"
#include "render/object_sprite_router.hpp"

#include <optional>
#include <vector>

namespace tibia::render {

struct MapItemSpriteRoute {
    MapItemSpriteLayout layout;
    std::vector<map::ObjectSpriteRouteBatch> batches;
};

enum class MapItemVisitRouteStatus : std::uint8_t {
    complete,
    nothing_to_route,
    waiting_for_effects,
    unresolved_render_inputs,
    unresolved_sprite_layout,
    unresolved_cell_data
};

struct MapItemObjectSpriteRoute {
    const map::Object* object{};
    MapItemSpriteRoute route;
    bool deferred{};
};

struct MapItemVisitRouteBatch {
    MapItemVisitRouteStatus status{MapItemVisitRouteStatus::nothing_to_route};
    std::vector<MapItemObjectSpriteRoute> routes;
};

[[nodiscard]] std::optional<MapItemSpriteRoute> route_map_item_sprites(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t anchor_x,
    std::int32_t anchor_y,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    std::int32_t maximum_cell_value,
    const map::TileRenderCellLookup& cell_lookup);

[[nodiscard]] MapItemVisitRouteBatch route_map_item_visit_sprite_objects(
    const map::MapItemRenderVisit& visit,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    const map::TileRenderCellLookup& cell_lookup);

[[nodiscard]] MapItemVisitRouteBatch route_deferred_map_item_visit(
    const map::MapItemRenderVisit& visit,
    std::int32_t effect_index,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    const map::TileRenderCellLookup& cell_lookup);

[[nodiscard]] std::optional<MapItemSpriteRoute> route_map_item_sprites(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    const MapItemSpriteVariantInputs& variant_inputs,
    std::int32_t anchor_x,
    std::int32_t anchor_y,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    std::int32_t maximum_cell_value,
    const map::TileRenderCellLookup& cell_lookup);

}
