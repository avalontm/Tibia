#include "map/item_render_traversal.hpp"
#include "map/tile_object_order.hpp"

#include "map/render_queue.hpp"
#include "resources/tibia_dat.hpp"

#include <algorithm>

namespace tibia::map {
namespace {

const Tile empty_map_tile{};

const Tile* find_viewport_tile(const DecodedMap& map,
    std::int32_t local_x, std::int32_t local_y, std::uint8_t floor,
    bool& duplicate) noexcept {
    const Tile* result = nullptr;
    for (const auto& tile : map.tiles) {
        const auto position = viewport_map_position(map, tile);
        if (!position || position->x != local_x || position->y != local_y ||
            position->floor != floor)
            continue;
        if (result != nullptr) {
            duplicate = true;
            return nullptr;
        }
        result = &tile;
    }
    return result == nullptr && !duplicate ? &empty_map_tile : result;
}

bool classify_visit(MapItemRenderVisit& visit,
    const resources::TibiaDatFile& dat,
    const std::vector<const Object*>& objects,
    MapItemTraversalStatus& status) {
    bool needs_neighbors = false;
    std::int32_t anchor_adjustment = 0;
    for (const auto* object : objects) {
        if (object->creature_id != 0 || object->type == 0 || object->type == 0x63)
            continue;
        const auto* item = dat.find_item(object->type);
        if (item == nullptr) {
            status = MapItemTraversalStatus::unresolved_dat_object;
            return false;
        }
        if (item->has_native_property(3)) {
            visit.deferred_object = object;
        } else if (!visit.neighboring_object_pass) {
            visit.sprite_objects.push_back({object, anchor_adjustment});
            if (item->has_native_property(27)) needs_neighbors = true;
        }
        if (item->has_native_property(0x1a)) {
            const auto* height = item->find_attribute(0x1a);
            if (height == nullptr || height->payload_words.size() != 1) {
                status = MapItemTraversalStatus::unresolved_dat_object;
                return false;
            }
            anchor_adjustment = std::min<std::int32_t>(
                anchor_adjustment + height->payload_words[0], 0x18);
        }
    }
    return needs_neighbors;
}

bool append_visit(MapItemTraversal& traversal, const DecodedMap& map,
    const resources::TibiaDatFile& dat, std::int32_t local_x,
    std::int32_t local_y, std::uint8_t floor, std::int32_t anchor_x,
    std::int32_t anchor_y, bool neighboring_object_pass) {
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y) {
        traversal.status = MapItemTraversalStatus::unresolved_tile;
        return false;
    }

    bool duplicate = false;
    const auto* tile = find_viewport_tile(map, local_x, local_y, floor, duplicate);
    if (tile == nullptr) {
        traversal.status = MapItemTraversalStatus::unresolved_tile;
        return false;
    }

    traversal.visits.push_back({tile, local_x, local_y, anchor_x, anchor_y,
        floor, neighboring_object_pass, {}, {}, nullptr});
    auto& visit = traversal.visits.back();
    auto objects = resolve_native_tile_object_order(*tile, dat);
    if (!objects) {
        traversal.status = MapItemTraversalStatus::unresolved_dat_object;
        return false;
    }
    visit.ordered_objects = *objects;
    const auto needs_neighbors = classify_visit(visit, dat, *objects, traversal.status);
    if (traversal.status != MapItemTraversalStatus::complete) return false;
    if (neighboring_object_pass || !needs_neighbors) return true;

    if (local_x > 0 && local_y > 0 &&
        !append_visit(traversal, map, dat, local_x - 1, local_y - 1, floor,
            anchor_x - 8, anchor_y - 8, true))
        return false;
    if (local_x > 0 &&
        !append_visit(traversal, map, dat, local_x - 1, local_y, floor,
            anchor_x - 8, anchor_y, true))
        return false;
    if (local_y > 0 &&
        !append_visit(traversal, map, dat, local_x, local_y - 1, floor,
            anchor_x, anchor_y - 8, true))
        return false;
    return true;
}

}

MapItemTraversal collect_map_item_render_visits(
    const DecodedMap& map,
    const resources::TibiaDatFile& dat,
    std::int32_t local_x,
    std::int32_t local_y,
    std::uint8_t floor,
    std::int32_t anchor_x,
    std::int32_t anchor_y) {
    MapItemTraversal traversal;
    if (floor > map_render_maximum_floor) {
        traversal.status = MapItemTraversalStatus::unresolved_tile;
        return traversal;
    }
    if (!append_visit(traversal, map, dat, local_x, local_y, floor,
        anchor_x, anchor_y, false))
        return traversal;
    return traversal;
}

std::optional<DeferredMapItemDraw> resolve_deferred_map_item_draw(
    const MapItemRenderVisit& visit,
    std::int32_t effect_index) noexcept {
    if (effect_index != -1 || visit.deferred_object == nullptr)
        return std::nullopt;
    return DeferredMapItemDraw{
        visit.deferred_object,
        visit.anchor_x,
        visit.anchor_y
    };
}

}
