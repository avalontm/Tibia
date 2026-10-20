#include "client/battle_panel_model.hpp"

#include <algorithm>
#include <cstdlib>

namespace tibia::client {
namespace {

std::uint8_t read_native_u8(
    const network::State6CreatureRuntimeState& creature, std::uint16_t offset) noexcept {
    if (offset < network::State6CreatureRuntimeState::first_native_offset) return 0;
    const auto index = static_cast<std::size_t>(
        offset - network::State6CreatureRuntimeState::first_native_offset);
    if (index >= creature.native_fields.size()) return 0;
    return creature.native_fields[index];
}

std::uint32_t read_native_u32(
    const network::State6CreatureRuntimeState& creature, std::uint16_t offset) noexcept {
    if (offset < network::State6CreatureRuntimeState::first_native_offset) return 0;
    const auto index = static_cast<std::size_t>(
        offset - network::State6CreatureRuntimeState::first_native_offset);
    if (index + 4 > creature.native_fields.size()) return 0;
    const auto& fields = creature.native_fields;
    return static_cast<std::uint32_t>(fields[index]) |
        (static_cast<std::uint32_t>(fields[index + 1]) << 8U) |
        (static_cast<std::uint32_t>(fields[index + 2]) << 16U) |
        (static_cast<std::uint32_t>(fields[index + 3]) << 24U);
}

}

std::optional<std::vector<BattleCreatureRow>> BattlePanelModel::update(
    const GameWorldSnapshot& snapshot) {
    if (!snapshot.map || snapshot.map->local_player_id == 0) {
        clear();
        return std::nullopt;
    }

    const auto local_player = std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(),
        [&snapshot](const auto& creature) {
            return creature.state.creature_id == snapshot.map->local_player_id;
        });
    if (local_player == snapshot.creatures.end()) {
        clear();
        return std::nullopt;
    }

    const auto creature_for = [&snapshot](std::uint32_t id) {
        return std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(),
            [id](const auto& creature) { return creature.state.creature_id == id; });
    };
    const auto qualifies = [&](const GameWorldCreatureSnapshot& creature) {
        const auto id = creature.state.creature_id;
        if (id == 0 || id == snapshot.map->local_player_id ||
            read_native_u8(creature.state, 0x90) == 0 ||
            read_native_u8(creature.state, 0x88) == 0)
            return false;

        if (id >= 0x40000000U) {
            const auto has_outfit = read_native_u32(creature.state, 0x60) != 0 ||
                read_native_u32(creature.state, 0x64) != 0;
            const auto same_floor = creature.world_floor == local_player->world_floor;
            const auto within_x = std::abs(static_cast<int>(creature.world_x) -
                static_cast<int>(local_player->world_x)) < 8;
            const auto within_y = std::abs(static_cast<int>(creature.world_y) -
                static_cast<int>(local_player->world_y)) < 6;
            if (!has_outfit || !same_floor || !within_x || !within_y) return false;
        }
        return true;
    };
    const auto local_party_shield = read_native_u8(local_player->state, 0x98);

    for (const auto id : snapshot.battle_creature_slots) {
        if (id == 0) continue;
        const auto creature = creature_for(id);
        const auto eligible = creature != snapshot.creatures.end() && qualifies(*creature);
        const auto visible = std::find(visible_creature_order_.begin(),
            visible_creature_order_.end(), id);
        if (eligible && visible == visible_creature_order_.end())
            visible_creature_order_.push_back(id);
        else if (!eligible && visible != visible_creature_order_.end())
            visible_creature_order_.erase(visible);
    }

    std::erase_if(visible_creature_order_, [&](std::uint32_t id) {
        return std::find(snapshot.battle_creature_slots.begin(),
            snapshot.battle_creature_slots.end(), id) == snapshot.battle_creature_slots.end();
    });

    std::vector<BattleCreatureRow> rows;
    rows.reserve(visible_creature_order_.size());
    for (const auto id : visible_creature_order_) {
        const auto creature = creature_for(id);
        if (creature == snapshot.creatures.end()) continue;
        const auto& entry = *creature;
        const auto skull = read_native_u8(entry.state, 0x94);
        const auto party_shield = read_native_u8(entry.state, 0x98);
        std::optional<std::uint16_t> skull_skin;
        if (skull >= 1 && skull <= 4)
            skull_skin = static_cast<std::uint16_t>(0xf4 + skull);
        std::optional<std::uint16_t> party_shield_skin;
        if (party_shield >= 2 && party_shield <= 4)
            party_shield_skin = static_cast<std::uint16_t>(0xfc + party_shield);
        else if (party_shield == 1 && local_party_shield != 1 &&
            local_party_shield != 3 && local_party_shield != 4)
            party_shield_skin = 0xfd;
        rows.push_back({
            entry.state,
            entry.world_x,
            entry.world_y,
            entry.world_floor,
            entry.name,
            read_native_u8(entry.state, 0x88),
            skull,
            party_shield,
            skull_skin,
            party_shield_skin
        });
    }
    return rows;
}

void BattlePanelModel::clear() noexcept {
    visible_creature_order_.clear();
}

}
