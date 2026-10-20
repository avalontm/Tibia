#pragma once

#include "client/game_world_snapshot.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tibia::client {

struct BattleCreatureRow {
    network::State6CreatureRuntimeState creature;
    std::uint16_t world_x{};
    std::uint16_t world_y{};
    std::uint8_t world_floor{};
    std::string name;
    std::uint8_t health_percent{};
    std::uint8_t skull{};
    std::uint8_t party_shield{};
    std::optional<std::uint16_t> skull_skin;
    std::optional<std::uint16_t> party_shield_skin;
};

class BattlePanelModel {
public:
    [[nodiscard]] std::optional<std::vector<BattleCreatureRow>> update(
        const GameWorldSnapshot& snapshot);
    void clear() noexcept;

private:
    std::vector<std::uint32_t> visible_creature_order_;
};

}
