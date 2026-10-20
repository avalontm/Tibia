#pragma once

#include "client/game_world_snapshot.hpp"
#include "render/creature_render_order.hpp"
#include "resources/tibia_dat.hpp"

#include <cstdint>
#include <vector>

namespace tibia::client {

struct CreatureRenderPlanEntry {
    std::int32_t local_x{};
    std::int32_t local_y{};
    render::CreatureRenderOrderEntry order;
};

struct CreatureRenderPlan {
    std::vector<CreatureRenderPlanEntry> ordered_creatures;
    std::vector<std::uint32_t> unresolved_creature_ids;
};

[[nodiscard]] CreatureRenderPlan prepare_creature_render_plan(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::uint8_t render_floor,
    std::int32_t reference_tile_x,
    std::int32_t reference_tile_y);

}
