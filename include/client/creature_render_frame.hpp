#pragma once

#include "client/creature_render_batch.hpp"
#include "client/creature_render_canvas.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tibia::client {

struct CreatureRenderFrameResult {
    std::size_t drawn_creatures{};
    std::size_t composed_creatures{};
    std::size_t unresolved_creatures{};
};

[[nodiscard]] CreatureRenderFrameResult render_creature_frame(
    render::SoftwareSurface& canvas,
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::uint8_t render_floor,
    std::int32_t reference_tile_x,
    std::int32_t reference_tile_y,
    std::int32_t origin_x,
    std::int32_t origin_y,
    std::uint32_t animation_clock,
    std::uint32_t current_tick,
    std::uint32_t animation_epoch_tick);

}
