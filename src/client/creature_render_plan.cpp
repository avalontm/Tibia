#include "client/creature_render_plan.hpp"

#include "client/creature_outfit_appearance.hpp"
#include "render/creature_projection.hpp"
#include "render/object_sprite_layout.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <numeric>

namespace tibia::client {
namespace {

constexpr std::size_t creature_candidates_per_tile = 10;

std::int64_t ceiling_divide_tile(std::int32_t value) noexcept {
    const auto widened = static_cast<std::int64_t>(value);
    return widened >= 0 ? (widened + 31) / 32 : widened / 32;
}

}

CreatureRenderPlan prepare_creature_render_plan(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::uint8_t render_floor,
    std::int32_t reference_tile_x,
    std::int32_t reference_tile_y) {
    CreatureRenderPlan plan;
    plan.ordered_creatures.reserve(snapshot.creatures.size());
    plan.unresolved_creature_ids.reserve(snapshot.creatures.size());
    if (!snapshot.map) {
        for (const auto& creature : snapshot.creatures)
            plan.unresolved_creature_ids.push_back(creature.state.creature_id);
        return plan;
    }

    constexpr auto tile_count = map::map_viewport_tiles_x * map::map_viewport_tiles_y;
    std::array<std::vector<render::CreatureRenderOrderEntry>, tile_count> tile_candidates;
    std::vector<std::size_t> source_order(snapshot.creatures.size());
    std::iota(source_order.begin(), source_order.end(), 0);
    std::stable_sort(source_order.begin(), source_order.end(), [&](auto left, auto right) {
        const auto& left_position = snapshot.creatures[left].viewport_map_position;
        const auto& right_position = snapshot.creatures[right].viewport_map_position;
        if (!left_position || !right_position) return left_position.has_value();
        if (left_position->x != right_position->x)
            return left_position->x < right_position->x;
        return left_position->y < right_position->y;
    });

    for (const auto index : source_order) {
        const auto& creature = snapshot.creatures[index];
        if (!creature.viewport_map_position) {
            plan.unresolved_creature_ids.push_back(creature.state.creature_id);
            continue;
        }
        if (creature.viewport_map_position->floor != render_floor) continue;
        const auto appearance = resolve_creature_outfit_appearance(creature.state, dat);
        if (!appearance) {
            plan.unresolved_creature_ids.push_back(creature.state.creature_id);
            continue;
        }
        const auto extent = render::resolve_object_sprite_extent(*appearance->outfit);
        const auto movement = render::resolve_creature_movement_offset(
            creature.state, snapshot.map->local_player_id, snapshot.local_player_movement);
        if (!extent || !movement) {
            plan.unresolved_creature_ids.push_back(creature.state.creature_id);
            continue;
        }
        const auto position = render::project_creature_screen_position(
            creature.viewport_map_position->x, creature.viewport_map_position->y,
            reference_tile_x, reference_tile_y, *movement, extent->x, extent->y);
        if (!position) {
            plan.unresolved_creature_ids.push_back(creature.state.creature_id);
            continue;
        }
        const auto tile_x = reference_tile_x + ceiling_divide_tile(position->x);
        const auto tile_y = reference_tile_y + ceiling_divide_tile(position->y);
        if (tile_x < 0 || tile_x >= static_cast<std::int32_t>(map::map_viewport_tiles_x) ||
            tile_y < 0 || tile_y >= static_cast<std::int32_t>(map::map_viewport_tiles_y))
            continue;
        if (tile_x - creature.viewport_map_position->x < -1 ||
            tile_x - creature.viewport_map_position->x > 1 ||
            tile_y - creature.viewport_map_position->y < -1 ||
            tile_y - creature.viewport_map_position->y > 1)
            continue;
        auto& candidates = tile_candidates[static_cast<std::size_t>(tile_x) *
            map::map_viewport_tiles_y + static_cast<std::size_t>(tile_y)];
        if (candidates.size() >= creature_candidates_per_tile) continue;
        candidates.push_back({position->x, position->y, index});
    }

    for (auto& candidates : tile_candidates) {
        render::sort_creature_render_order(candidates);
    }
    for (std::int32_t tile_x = 0;
        tile_x < static_cast<std::int32_t>(map::map_viewport_tiles_x); ++tile_x) {
        for (std::int32_t tile_y = 0;
            tile_y < static_cast<std::int32_t>(map::map_viewport_tiles_y); ++tile_y) {
            auto& candidates = tile_candidates[static_cast<std::size_t>(tile_x) *
                map::map_viewport_tiles_y + static_cast<std::size_t>(tile_y)];
            for (auto& candidate : candidates)
                plan.ordered_creatures.push_back({tile_x, tile_y, std::move(candidate)});
        }
    }
    return plan;
}

}
