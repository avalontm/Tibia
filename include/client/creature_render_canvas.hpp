#pragma once

#include "client/creature_render_batch.hpp"
#include "client/map_window_layout.hpp"

#include <cstddef>
#include <cstdint>

namespace tibia::client {

[[nodiscard]] std::size_t draw_creature_render_batch(
    render::SoftwareSurface& canvas,
    const CreatureRenderBatch& batch,
    std::int32_t origin_x,
    std::int32_t origin_y);

[[nodiscard]] std::size_t draw_creature_render_batch(
    render::SoftwareSurface& canvas,
    const CreatureRenderBatch& batch,
    const MapWindowLayout& layout);

}
