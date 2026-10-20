#include "client/creature_render_batch.hpp"

#include "client/creature_outfit_appearance.hpp"

#include <utility>

namespace tibia::client {

CreatureRenderBatch compose_creature_render_batch(
    const GameWorldSnapshot& snapshot,
    const CreatureRenderPlan& plan,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) {
    CreatureRenderBatch batch;
    batch.entries.reserve(plan.ordered_creatures.size());
    batch.unresolved_creature_ids = plan.unresolved_creature_ids;

    for (const auto& ordered : plan.ordered_creatures) {
        if (ordered.order.source_index >= snapshot.creatures.size()) continue;

        const auto& creature = snapshot.creatures[ordered.order.source_index];
        auto surface = compose_creature_outfit_from_state(
            tibia_spr_bytes,
            tibia_spr,
            creature.state,
            dat,
            snapshot.map ? snapshot.map->local_player_id : 0,
            snapshot.local_player_movement,
            animation_clock,
            elapsed_since_animation_epoch);
        if (!surface) {
            batch.unresolved_creature_ids.push_back(creature.state.creature_id);
            continue;
        }

        batch.entries.push_back({
            creature.state.creature_id,
            ordered.local_x,
            ordered.local_y,
            ordered.order.screen_x,
            ordered.order.screen_y,
            creature.name,
            creature.state.native_fields[0x58],
            creature.state.native_fields[0x48],
            creature.state.native_fields[0x64],
            creature.state.native_fields[0x68],
            creature.state.native_fields[0x6c],
            snapshot.map && creature.state.creature_id == snapshot.map->local_player_id,
            std::move(*surface)
        });
    }

    return batch;
}

}
