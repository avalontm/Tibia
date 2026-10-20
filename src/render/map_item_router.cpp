#include "render/map_item_router.hpp"

#include <utility>

namespace tibia::render {
namespace {

MapItemVisitRouteStatus route_status_for(
    const MapItemSpriteRoute& route) noexcept {
    for (const auto& batch : route.batches)
        if (batch.status == map::ObjectSpriteRouteStatus::unresolved_cell_data)
            return MapItemVisitRouteStatus::unresolved_cell_data;
    return MapItemVisitRouteStatus::complete;
}

std::optional<MapItemSpriteVariantInputs> resolve_visit_variant_inputs(
    const map::Object& object,
    const resources::TibiaDatFile& dat) {
    return resolve_map_item_sprite_variant_inputs(object, dat);
}

}

std::optional<MapItemSpriteRoute> route_map_item_sprites(
    const map::Object& map_object,
    const map::Tile& tile,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t anchor_x,
    std::int32_t anchor_y,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    std::int32_t maximum_cell_value,
    const map::TileRenderCellLookup& cell_lookup) {
    return route_map_item_sprites(
        map_object, tile, dat, animation_clock, {},
        anchor_x, anchor_y, camera_offset_x, camera_offset_y,
        maximum_cell_value, cell_lookup);
}

std::optional<MapItemSpriteRoute> route_map_item_sprites(
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
    const map::TileRenderCellLookup& cell_lookup) {
    auto layout = resolve_map_item_sprite_layout(
        map_object, tile, dat, animation_clock, variant_inputs);
    if (!layout) return std::nullopt;

    auto batches = route_object_sprite_layout(
        anchor_x, anchor_y,
        camera_offset_x, camera_offset_y,
        maximum_cell_value,
        layout->placements,
        cell_lookup);
    return MapItemSpriteRoute{std::move(*layout), std::move(batches)};
}

MapItemVisitRouteBatch route_map_item_visit_sprite_objects(
    const map::MapItemRenderVisit& visit,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    const map::TileRenderCellLookup& cell_lookup) {
    MapItemVisitRouteBatch result;
    if (visit.sprite_objects.empty()) return result;
    result.status = MapItemVisitRouteStatus::complete;
    for (const auto& sprite_object : visit.sprite_objects) {
        const auto* object = sprite_object.object;
        if (object == nullptr || visit.tile == nullptr) {
            result.status = MapItemVisitRouteStatus::unresolved_render_inputs;
            return result;
        }
        auto variant_inputs = resolve_visit_variant_inputs(
            *object, dat);
        if (!variant_inputs) {
            result.status = MapItemVisitRouteStatus::unresolved_render_inputs;
            return result;
        }
        variant_inputs->position = MapItemSpriteVariantInputs::Position{
            visit.local_x, visit.local_y, visit.floor};
        auto route = route_map_item_sprites(
            *object, *visit.tile, dat, animation_clock, *variant_inputs,
            visit.anchor_x - sprite_object.anchor_adjustment,
            visit.anchor_y - sprite_object.anchor_adjustment,
            camera_offset_x, camera_offset_y, visit.floor,
            cell_lookup);
        if (!route) {
            result.status = MapItemVisitRouteStatus::unresolved_sprite_layout;
            return result;
        }
        result.status = route_status_for(*route);
        result.routes.push_back({object, std::move(*route), false});
        if (result.status != MapItemVisitRouteStatus::complete) return result;
    }
    return result;
}

MapItemVisitRouteBatch route_deferred_map_item_visit(
    const map::MapItemRenderVisit& visit,
    std::int32_t effect_index,
    const resources::TibiaDatFile& dat,
    std::uint32_t animation_clock,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    const map::TileRenderCellLookup& cell_lookup) {
    MapItemVisitRouteBatch result;
    if (visit.deferred_object == nullptr) return result;
    if (effect_index != -1) {
        result.status = MapItemVisitRouteStatus::waiting_for_effects;
        return result;
    }
    if (visit.tile == nullptr) {
        result.status = MapItemVisitRouteStatus::unresolved_render_inputs;
        return result;
    }
    auto variant_inputs = resolve_visit_variant_inputs(
        *visit.deferred_object, dat);
    if (!variant_inputs) {
        result.status = MapItemVisitRouteStatus::unresolved_render_inputs;
        return result;
    }
    variant_inputs->position = MapItemSpriteVariantInputs::Position{
        visit.local_x, visit.local_y, visit.floor};
    auto route = route_map_item_sprites(
        *visit.deferred_object, *visit.tile, dat, animation_clock,
        *variant_inputs, visit.anchor_x, visit.anchor_y,
        camera_offset_x, camera_offset_y, visit.floor,
        cell_lookup);
    if (!route) {
        result.status = MapItemVisitRouteStatus::unresolved_sprite_layout;
        return result;
    }
    result.status = route_status_for(*route);
    result.routes.push_back({visit.deferred_object, std::move(*route), true});
    return result;
}

}
