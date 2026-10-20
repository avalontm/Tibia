#include "client/creature_render_frame.hpp"

#include "client/creature_render_plan.hpp"

namespace tibia::client {

CreatureRenderFrameResult render_creature_frame(
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
    std::uint32_t animation_epoch_tick) {
    const auto plan = prepare_creature_render_plan(
        snapshot, dat, render_floor, reference_tile_x, reference_tile_y);
    const auto batch = compose_creature_render_batch(
        snapshot, plan, dat, tibia_spr_bytes, tibia_spr,
        animation_clock, current_tick - animation_epoch_tick);
    return {
        draw_creature_render_batch(canvas, batch, origin_x, origin_y),
        batch.entries.size(),
        batch.unresolved_creature_ids.size()
    };
}

}
