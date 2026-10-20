#pragma once

#include "client/game_world_snapshot.hpp"
#include "navigation/navigation.hpp"
#include "resources/tibia_dat.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <span>
#include <vector>

namespace tibia::client {

inline constexpr std::uint8_t game_stop_movement_opcode =
    network::state6_stop_action_opcode;

enum class GameMovementKey : std::uint8_t {
    north,
    west,
    east,
    south
};

struct GameMovementRequest {
    std::vector<std::uint8_t> payload;
    bool target_blocked{};
    std::string status_message;
};

struct GameMovementInputState {
    std::array<std::uint32_t, 3> last_target{};
    std::uint32_t local_player_id{};
    std::uint8_t option_0x05A09{};
    std::uint8_t option_0x05A08{};
    std::uint32_t queued_step_count{};
    std::uint32_t current_step_index{};
    std::uint32_t pending_path_search{};
    bool has_route_progress{};
    bool has_last_target{};
};

[[nodiscard]] std::optional<GameMovementRequest> make_game_movement_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    GameMovementKey key,
    GameMovementInputState& input_state);

[[nodiscard]] std::optional<GameMovementRequest> make_map_walk_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    network::State6PlayerPosition target,
    std::span<const network::State6PathSearchCell> path_grid,
    GameMovementInputState& input_state);

[[nodiscard]] std::optional<GameMovementRequest> continue_map_walk_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const network::State6PathSearchCell> path_grid,
    std::uint32_t animation_tick,
    GameMovementInputState& input_state);

}
