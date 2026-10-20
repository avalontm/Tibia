#pragma once

#include "map/render_queue.hpp"
#include "render/object_sprite_layout.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tibia::render {

[[nodiscard]] std::vector<map::ObjectSpriteRouteBatch> route_object_sprite_layout(
    std::int32_t anchor_x, std::int32_t anchor_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t maximum_cell_value,
    std::span<const ObjectSpritePlacement> placements,
    const map::TileRenderCellLookup& cell_lookup);

}
