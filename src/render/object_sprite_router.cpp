#include "render/object_sprite_router.hpp"

#include <utility>

namespace tibia::render {

std::vector<map::ObjectSpriteRouteBatch> route_object_sprite_layout(
    std::int32_t anchor_x, std::int32_t anchor_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t maximum_cell_value,
    std::span<const ObjectSpritePlacement> placements,
    const map::TileRenderCellLookup& cell_lookup) {
    std::vector<map::ObjectSpriteRouteBatch> batches;
    batches.reserve(placements.size());
    for (const auto& placement : placements) {
        auto batch = map::route_object_sprite(
            anchor_x + placement.offset_x,
            anchor_y + placement.offset_y,
            camera_offset_x,
            camera_offset_y,
            placement.sprite_id,
            placement.bypass_tile_cache,
            maximum_cell_value,
            cell_lookup);
        batches.push_back(std::move(batch));
        if (batches.back().status == map::ObjectSpriteRouteStatus::unresolved_cell_data)
            break;
    }
    return batches;
}

}
