#include "client/game_movement_request.hpp"

#include "navigation/navigation.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace tibia::client {
namespace {

struct MovementKeyData {
    std::int32_t delta_x{};
    std::int32_t delta_y{};
};

MovementKeyData movement_key_data(GameMovementKey key) noexcept {
    switch (key) {
    case GameMovementKey::north: return {0, -1};
    case GameMovementKey::west: return {-1, 0};
    case GameMovementKey::east: return {1, 0};
    case GameMovementKey::south: return {0, 1};
    }
    return {};
}

void restore_route_progress(GameMovementInputState& input_state,
    network::State6LocalPlayerMovementGlobals& movement_state) noexcept {
    if (!input_state.has_route_progress) return;
    movement_state.at_0x006059DC = input_state.queued_step_count;
    movement_state.at_0x006059D8 = input_state.current_step_index;
    movement_state.at_0x006059D4 = input_state.pending_path_search;
}

void save_route_progress(GameMovementInputState& input_state,
    const network::State6LocalPlayerMovementGlobals& movement_state) noexcept {
    input_state.queued_step_count = movement_state.at_0x006059DC;
    input_state.current_step_index = movement_state.at_0x006059D8;
    input_state.pending_path_search = movement_state.at_0x006059D4;
    input_state.has_route_progress = true;
}

}

std::optional<GameMovementRequest> make_game_movement_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    GameMovementKey key,
    GameMovementInputState& input_state) {
    if (!snapshot.map || snapshot.map->local_player_id == 0) return std::nullopt;
    const auto player = std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(),
        [&snapshot](const auto& creature) {
            return creature.state.creature_id == snapshot.map->local_player_id;
        });
    if (player == snapshot.creatures.end()) return std::nullopt;

    const auto key_data = movement_key_data(key);
    const auto target_x = static_cast<std::int64_t>(player->world_x) + key_data.delta_x;
    const auto target_y = static_cast<std::int64_t>(player->world_y) + key_data.delta_y;
    if (target_x < 0 || target_y < 0 || target_x > 0xffff || target_y > 0xffff)
        return std::nullopt;

    const network::State6PlayerPosition current{
        player->world_x, player->world_y, player->world_floor};
    if (input_state.local_player_id != snapshot.map->local_player_id) {
        input_state = {};
        input_state.local_player_id = snapshot.map->local_player_id;
    }
    auto movement_state = snapshot.local_player_movement;
    restore_route_progress(input_state, movement_state);
    movement_state.at_0x00605A14 = input_state.last_target[0];
    movement_state.at_0x00605A10 = input_state.last_target[1];
    movement_state.at_0x00605A0C = input_state.last_target[2];
    movement_state.at_0x00605A09 = input_state.option_0x05A09;
    movement_state.at_0x00605A08 = input_state.option_0x05A08;
    const network::State6PlayerPosition target{
        static_cast<std::uint32_t>(target_x),
        static_cast<std::uint32_t>(target_y), player->world_floor};
    const network::State6PathTargetRequest path_target{target, 1, 0};
    const auto evaluation = network::evaluate_observed_state6_path_request(
        movement_state, {current, path_target, std::nullopt}, *snapshot.map, dat);

    input_state.last_target = {movement_state.at_0x00605A14,
        movement_state.at_0x00605A10, movement_state.at_0x00605A0C};
    input_state.option_0x05A09 = movement_state.at_0x00605A09;
    input_state.option_0x05A08 = movement_state.at_0x00605A08;
    save_route_progress(input_state, movement_state);
    input_state.has_last_target =
        evaluation.disposition != network::State6PathRequestDisposition::not_possible &&
        (input_state.last_target[0] != 0 || input_state.last_target[1] != 0 ||
            input_state.last_target[2] != 0);

    if (evaluation.disposition == network::State6PathRequestDisposition::not_possible)
        return GameMovementRequest{{}, true};
    if (evaluation.disposition == network::State6PathRequestDisposition::route_pending)
        return GameMovementRequest{{game_stop_movement_opcode}, false};
    if (evaluation.disposition != network::State6PathRequestDisposition::request_path_search)
        return std::nullopt;

    const network::State6PathRequest path_request{current, target, false};
    const auto precheck = network::precheck_observed_state6_path_request(path_request);
    if (!precheck.direct_direction) return std::nullopt;
    const auto command = network::make_observed_state6_movement_command(
        std::span<const std::uint8_t>(&*precheck.direct_direction, 1));
    if (!command) return std::nullopt;

    (void)network::complete_observed_state6_path_search(movement_state, 1, 1);
    input_state.last_target = {movement_state.at_0x00605A14,
        movement_state.at_0x00605A10, movement_state.at_0x00605A0C};
    input_state.option_0x05A09 = movement_state.at_0x00605A09;
    input_state.option_0x05A08 = movement_state.at_0x00605A08;
    input_state.has_last_target = movement_state.at_0x00605A14 != 0;
    save_route_progress(input_state, movement_state);

    std::vector<std::uint8_t> payload;
    payload.reserve(command->payload.size() + 1);
    payload.push_back(command->opcode);
    payload.insert(payload.end(), command->payload.begin(), command->payload.end());
    return GameMovementRequest{std::move(payload), false};
}

std::optional<GameMovementRequest> make_map_walk_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    network::State6PlayerPosition target,
    std::span<const network::State6PathSearchCell> path_grid,
    GameMovementInputState& input_state) {
    if (!snapshot.map || snapshot.map->local_player_id == 0) return std::nullopt;
    const auto player = std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(),
        [&snapshot](const auto& creature) {
            return creature.state.creature_id == snapshot.map->local_player_id;
        });
    if (player == snapshot.creatures.end()) return std::nullopt;

    const network::State6PlayerPosition current{
        player->world_x, player->world_y, player->world_floor};
    if (input_state.local_player_id != snapshot.map->local_player_id) {
        input_state = {};
        input_state.local_player_id = snapshot.map->local_player_id;
    }
    auto movement_state = snapshot.local_player_movement;
    restore_route_progress(input_state, movement_state);
    movement_state.at_0x00605A14 = input_state.last_target[0];
    movement_state.at_0x00605A10 = input_state.last_target[1];
    movement_state.at_0x00605A0C = input_state.last_target[2];
    movement_state.at_0x00605A09 = input_state.option_0x05A09;
    movement_state.at_0x00605A08 = input_state.option_0x05A08;

    const network::State6PathTargetRequest path_target{target, 1, 0};
    const auto evaluation = network::evaluate_observed_state6_path_request(
        movement_state, {current, path_target, std::nullopt}, *snapshot.map, dat);
    input_state.last_target = {movement_state.at_0x00605A14,
        movement_state.at_0x00605A10, movement_state.at_0x00605A0C};
    input_state.option_0x05A09 = movement_state.at_0x00605A09;
    input_state.option_0x05A08 = movement_state.at_0x00605A08;
    save_route_progress(input_state, movement_state);
    input_state.has_last_target =
        evaluation.disposition != network::State6PathRequestDisposition::not_possible &&
        (input_state.last_target[0] != 0 || input_state.last_target[1] != 0 ||
            input_state.last_target[2] != 0);

    if (evaluation.disposition == network::State6PathRequestDisposition::not_possible)
        return GameMovementRequest{{}, true};
    if (evaluation.disposition == network::State6PathRequestDisposition::route_pending)
        return GameMovementRequest{{game_stop_movement_opcode}, false};
    if (evaluation.disposition != network::State6PathRequestDisposition::request_path_search)
        return std::nullopt;

    const auto path = network::search_observed_state6_path_grid(
        {current, target, false}, path_grid,
        path_target.option_0x05A09 != 0);
    if (!path) return std::nullopt;
    const auto completion = network::complete_observed_state6_path_search(
        movement_state, path->native_return,
        static_cast<std::uint32_t>(path->step_count));
    input_state.last_target = {movement_state.at_0x00605A14,
        movement_state.at_0x00605A10, movement_state.at_0x00605A0C};
    input_state.option_0x05A09 = movement_state.at_0x00605A09;
    input_state.option_0x05A08 = movement_state.at_0x00605A08;
    input_state.has_last_target = !completion.clear_target &&
        movement_state.at_0x00605A14 != 0;
    save_route_progress(input_state, movement_state);
    if (path->native_return == 0) return GameMovementRequest{};
    if (path->native_return < 0) {
        std::string status;
        switch (path->native_return) {
        case -1: status = "First go upstairs."; break;
        case -2: status = "First go downstairs."; break;
        case -3: status = "Destination is out of range."; break;
        case -4: status = "There is no way."; break;
        }
        return GameMovementRequest{{}, false, std::move(status)};
    }

    const auto command = network::make_observed_state6_movement_command(path->directions);
    if (!command) return std::nullopt;
    std::vector<std::uint8_t> payload;
    payload.reserve(command->payload.size() + 1);
    payload.push_back(command->opcode);
    payload.insert(payload.end(), command->payload.begin(), command->payload.end());
    return GameMovementRequest{std::move(payload), false};
}

std::optional<GameMovementRequest> continue_map_walk_request(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const network::State6PathSearchCell> path_grid,
    std::uint32_t animation_tick,
    GameMovementInputState& input_state) {
    if (!snapshot.map || snapshot.map->local_player_id == 0 ||
        input_state.local_player_id != snapshot.map->local_player_id ||
        !input_state.has_last_target || !input_state.has_route_progress ||
        input_state.current_step_index < input_state.queued_step_count)
        return std::nullopt;

    const auto player = std::find_if(snapshot.creatures.begin(), snapshot.creatures.end(),
        [&snapshot](const auto& creature) {
            return creature.state.creature_id == snapshot.map->local_player_id;
        });
    if (player == snapshot.creatures.end()) return std::nullopt;

    constexpr auto active_index = static_cast<std::size_t>(0x4C -
        network::State6CreatureRuntimeState::first_native_offset);
    constexpr auto full_end_index = static_cast<std::size_t>(0x3C -
        network::State6CreatureRuntimeState::first_native_offset);
    const auto& fields = player->state.native_fields;
    const auto full_end_tick = static_cast<std::uint32_t>(fields[full_end_index]) |
        (static_cast<std::uint32_t>(fields[full_end_index + 1]) << 8U) |
        (static_cast<std::uint32_t>(fields[full_end_index + 2]) << 16U) |
        (static_cast<std::uint32_t>(fields[full_end_index + 3]) << 24U);
    if (fields[active_index] != 0 &&
        static_cast<std::int32_t>(animation_tick - full_end_tick) < 0)
        return std::nullopt;

    const network::State6PlayerPosition current{
        player->world_x, player->world_y, player->world_floor};
    const network::State6PlayerPosition target{
        input_state.last_target[0], input_state.last_target[1],
        input_state.last_target[2]};
    if (!network::state6_path_target_is_near(current, target))
        return std::nullopt;

    auto movement_state = snapshot.local_player_movement;
    restore_route_progress(input_state, movement_state);
    movement_state.at_0x00605A14 = target.x;
    movement_state.at_0x00605A10 = target.y;
    movement_state.at_0x00605A0C = target.z;
    movement_state.at_0x00605A09 = input_state.option_0x05A09;
    movement_state.at_0x00605A08 = input_state.option_0x05A08;

    const auto path = network::search_observed_state6_path_grid(
        {current, target, false}, path_grid,
        input_state.option_0x05A09 != 0);
    if (!path) return std::nullopt;
    const auto completion = network::complete_observed_state6_path_search(
        movement_state, path->native_return,
        static_cast<std::uint32_t>(path->step_count));
    input_state.last_target = {movement_state.at_0x00605A14,
        movement_state.at_0x00605A10, movement_state.at_0x00605A0C};
    input_state.option_0x05A09 = movement_state.at_0x00605A09;
    input_state.option_0x05A08 = movement_state.at_0x00605A08;
    input_state.has_last_target = !completion.clear_target &&
        movement_state.at_0x00605A14 != 0;
    save_route_progress(input_state, movement_state);
    if (path->native_return == 0) return GameMovementRequest{};
    if (path->native_return < 0) {
        std::string status;
        switch (path->native_return) {
        case -1: status = "First go upstairs."; break;
        case -2: status = "First go downstairs."; break;
        case -3: status = "Destination is out of range."; break;
        case -4: status = "There is no way."; break;
        }
        return GameMovementRequest{{}, false, std::move(status)};
    }

    const auto command = network::make_observed_state6_movement_command(path->directions);
    if (!command) return std::nullopt;
    std::vector<std::uint8_t> payload;
    payload.reserve(command->payload.size() + 1);
    payload.push_back(command->opcode);
    payload.insert(payload.end(), command->payload.begin(), command->payload.end());
    return GameMovementRequest{std::move(payload), false};
}

}
