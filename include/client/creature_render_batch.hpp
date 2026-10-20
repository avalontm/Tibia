#pragma once

#include "client/creature_render_plan.hpp"
#include "render/software_surface.hpp"
#include "resources/tibia_spr.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tibia::client {

struct CreatureRenderBatchEntry {
    std::uint32_t creature_id{};
    std::int32_t local_x{};
    std::int32_t local_y{};
    std::int32_t sort_x{};
    std::int32_t sort_y{};
    std::string name;
    std::uint8_t health_percent{};
    std::uint8_t light_level{};
    std::uint8_t skull{};
    std::uint8_t party_shield{};
    std::uint8_t party_relation{};
    bool local_player{};
    render::SoftwareSurface surface;
};

struct CreatureRenderBatch {
    std::vector<CreatureRenderBatchEntry> entries;
    std::vector<std::uint32_t> unresolved_creature_ids;
};

[[nodiscard]] CreatureRenderBatch compose_creature_render_batch(
    const GameWorldSnapshot& snapshot,
    const CreatureRenderPlan& plan,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch);

}
