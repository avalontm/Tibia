#include "navigation/navigation.hpp"
#include "map/model.hpp"
#include "resources/tibia_dat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tibia::network {

State6B5StallEffect observe_state6_b5_position(State6B5StallState& state,
    State6PlayerPosition position) noexcept {
    if (state.previous_position_valid && state.previous_position == position)
        ++state.unchanged_position_calls;
    else
        state.unchanged_position_calls = 0;
    state.previous_position = position;
    state.previous_position_valid = true;
    const bool stop = state.unchanged_position_calls > 15;
    if (stop) state.unchanged_position_calls = 0;
    return {stop, state.unchanged_position_calls};
}

State6PathTargetUpdate update_observed_state6_path_target(
    tibia::navigation::LocalPlayerMovementState& globals, State6PlayerPosition current_position,
    const State6PathTargetRequest& request) noexcept {
    const bool same_as_previous = globals.at_0x00605A14 == request.target.x &&
        globals.at_0x00605A10 == request.target.y &&
        globals.at_0x00605A0C == request.target.z;
    const auto pending_x = static_cast<std::int64_t>(current_position.x) + globals.at_0x00605A20;
    const auto pending_y = static_cast<std::int64_t>(current_position.y) + globals.at_0x00605A1C;
    const bool same_as_pending_step = pending_x == request.target.x &&
        pending_y == request.target.y && current_position.z == request.target.z;
    if (same_as_previous || same_as_pending_step) return State6PathTargetUpdate::unchanged;

    globals.at_0x00605A14 = request.target.x;
    globals.at_0x00605A10 = request.target.y;
    globals.at_0x00605A0C = request.target.z;
    globals.at_0x00605A09 = request.option_0x05A09;
    globals.at_0x00605A08 = request.option_0x05A08;
    return State6PathTargetUpdate::changed;
}

bool state6_path_target_is_near(State6PlayerPosition current,
    State6PlayerPosition target) noexcept {
    const auto delta_x = static_cast<std::int64_t>(target.x) - current.x;
    const auto delta_y = static_cast<std::int64_t>(target.y) - current.y;
    const auto distance = (delta_x < 0 ? -delta_x : delta_x) +
        (delta_y < 0 ? -delta_y : delta_y);
    return distance < 2;
}

std::optional<State6MovementDelta> state6_movement_delta(std::uint8_t direction) noexcept {
    constexpr std::array<State6MovementDelta, 8> deltas{{
        {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
    if (direction == 0 || direction > deltas.size()) return std::nullopt;
    return deltas[direction - 1];
}

std::optional<State6CreatureMoveStateEffect> make_observed_state6_creature_move_state(
    tibia::navigation::LocalPlayerMovementState& globals,
    const State6CreatureMoveStateInput& input) noexcept {
    if (input.tick_quantum <= 0) return std::nullopt;

    auto direction = input.current_direction;
    if (input.delta_x > 0) direction = 1;
    else if (input.delta_x < 0) direction = 3;
    else if (input.delta_y < 0) direction = 0;
    else if (input.delta_y > 0) direction = 2;

    State6CreatureMoveStateEffect effect;
    effect.direction = direction;
    effect.offset_x_subtiles = static_cast<std::int32_t>(
        -static_cast<std::int64_t>(input.delta_x) * 0x20);
    effect.offset_y_subtiles = static_cast<std::int32_t>(
        -static_cast<std::int64_t>(input.delta_y) * 0x20);
    effect.distance_x_subtiles = static_cast<std::int32_t>(
        std::abs(static_cast<std::int64_t>(input.delta_x)) * 0x20);
    effect.distance_y_subtiles = static_cast<std::int32_t>(
        std::abs(static_cast<std::int64_t>(input.delta_y)) * 0x20);

    if (input.duration_parameter != 0 && input.creature_id == input.local_player_id) {
        const bool matches_pending = globals.at_0x00605A20 == input.delta_x &&
            globals.at_0x00605A1C == input.delta_y;
        globals.at_0x00605A20 = 0;
        globals.at_0x00605A1C = 0;
        if (matches_pending) {
            effect.returned_on_matching_pending_delta = true;
            return effect;
        }
        ++globals.at_0x006059D8;
        effect.local_route_index_incremented = true;
    }

    effect.previous_direction = direction;
    const auto rounded_duration = [&](std::int64_t multiplier) -> std::int64_t {
        const auto raw = input.creature_speed < 1
            ? std::int64_t{1000}
            : (static_cast<std::int64_t>(input.duration_parameter) * multiplier) /
                input.creature_speed;
        const auto quantum = static_cast<std::int64_t>(input.tick_quantum);
        return ((quantum - 1 + raw) / quantum) * quantum;
    };
    const auto step_duration = rounded_duration(1000);
    const auto full_duration = input.delta_x != 0 && input.delta_y != 0
        ? (input.creature_speed < 1 ? std::int64_t{1000}
            : (static_cast<std::int64_t>(input.duration_parameter) * 3000) /
                input.creature_speed)
        : step_duration;
    effect.step_duration_ms = static_cast<std::int32_t>(step_duration);
    effect.full_duration_ms = static_cast<std::int32_t>(full_duration);
    effect.step_end_tick = input.first_timer_sample +
        static_cast<std::uint32_t>(step_duration);
    effect.full_end_tick = input.second_timer_sample +
        static_cast<std::uint32_t>(full_duration);
    effect.movement_active = true;
    return effect;
}

std::uint8_t state6_native_path_floor_index(std::uint8_t current_floor) noexcept {
    return current_floor < 8 ? static_cast<std::uint8_t>(7 - current_floor) : 2;
}

State6PathPrecheck precheck_observed_state6_path_request(
    const State6PathRequest& request) noexcept {
    if (request.to.z < request.from.z) return {-1, std::nullopt, false};
    if (request.from.z < request.to.z) return {-2, std::nullopt, false};
    const auto dx = static_cast<std::int64_t>(request.to.x) - request.from.x;
    const auto dy = static_cast<std::int64_t>(request.to.y) - request.from.y;
    const auto abs_x = std::abs(dx);
    const auto abs_y = std::abs(dy);
    if (abs_x > 110 || abs_y > 110) return {-3, std::nullopt, false};
    if (dx == 0 && dy == 0) return {0, std::nullopt, false};

    std::optional<std::uint8_t> direction;
    if (abs_x + abs_y == 1) {
        if (dx == 1) direction = 1;
        else if (dx == -1) direction = 5;
        else if (dy == -1) direction = 3;
        else direction = 7;
    } else if (request.allow_direct_diagonal && abs_x == 1 && abs_y == 1) {
        if (dx == 1 && dy == -1) direction = 2;
        else if (dx == -1 && dy == -1) direction = 4;
        else if (dx == -1 && dy == 1) direction = 6;
        else direction = 8;
    }
    if (direction) return {1, direction, false};
    return {std::nullopt, std::nullopt, true};
}

std::optional<State6PathSearchResult> search_observed_state6_path_grid(
    const State6PathRequest& request, std::span<const State6PathSearchCell> cells,
    bool native_param_7) {
    constexpr std::int32_t radius = 110;
    constexpr std::size_t width = radius * 2U + 1U;
    constexpr std::size_t cell_count = width * width;
    if (cells.size() != cell_count) return std::nullopt;

    const auto precheck = precheck_observed_state6_path_request(request);
    if (precheck.native_return) {
        State6PathSearchResult result;
        result.native_return = *precheck.native_return;
        if (precheck.direct_direction) {
            result.step_count = 1;
            result.directions.push_back(*precheck.direct_direction);
        }
        return result;
    }

    struct Node {
        std::int16_t x{}, y{};
        bool blocked{};
        std::uint8_t weight{};
        std::int32_t distance{std::numeric_limits<std::int32_t>::max()};
        std::int32_t score{std::numeric_limits<std::int32_t>::max()};
        std::int32_t predecessor{-1};
    };
    std::vector<Node> nodes(cell_count);
    std::uint8_t minimum_weight = std::numeric_limits<std::uint8_t>::max();
    for (std::int32_t y = -radius; y <= radius; ++y) {
        for (std::int32_t x = -radius; x <= radius; ++x) {
            const auto index = static_cast<std::size_t>(y + radius) * width +
                static_cast<std::size_t>(x + radius);
            nodes[index] = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                cells[index].blocked, cells[index].terrain_cost};
            minimum_weight = std::min(minimum_weight, cells[index].terrain_cost);
        }
    }
    const auto index_of = [](std::int32_t x, std::int32_t y) noexcept {
        return static_cast<std::size_t>(y + radius) * width +
            static_cast<std::size_t>(x + radius);
    };
    const auto origin = index_of(0, 0);
    const auto target_x = static_cast<std::int32_t>(
        static_cast<std::int64_t>(request.to.x) - request.from.x);
    const auto target_y = static_cast<std::int32_t>(
        static_cast<std::int64_t>(request.to.y) - request.from.y);
    const auto target = index_of(target_x, target_y);
    nodes[target].distance = 0;

    std::vector<std::size_t> open{target};
    while (!open.empty()) {
        const auto current_index = open.front();
        open.erase(open.begin());
        const auto current = nodes[current_index];
        if (nodes[origin].distance <= current.distance + current.weight) break;

        for (std::int32_t offset_y = -1; offset_y <= 1; ++offset_y) {
            for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
                if (offset_x == 0 && offset_y == 0) continue;
                const auto next_x = static_cast<std::int32_t>(current.x) + offset_x;
                const auto next_y = static_cast<std::int32_t>(current.y) + offset_y;
                if (next_x < -radius || next_x > radius ||
                    next_y < -radius || next_y > radius) continue;
                const auto next_index = index_of(next_x, next_y);
                const auto step_cost = (std::abs(offset_x) + std::abs(offset_y) == 1)
                    ? 1 : static_cast<std::int32_t>(current.weight) * 2;
                const auto new_distance = current.distance + step_cost;
                auto& next = nodes[next_index];
                if (new_distance >= next.distance) continue;
                next.distance = new_distance;
                next.predecessor = static_cast<std::int32_t>(current_index);
                if ((next_x == 0 && next_y == 0) || next.blocked) continue;

                const auto manhattan = std::abs(next_x) + std::abs(next_y);
                next.score = new_distance + next.weight +
                    (manhattan - 1) * static_cast<std::int32_t>(minimum_weight);
                std::erase(open, next_index);
                const auto insertion = std::find_if(open.begin(), open.end(),
                    [&](std::size_t queued) { return nodes[queued].score >= next.score; });
                open.insert(insertion, next_index);
            }
        }
    }
    if (nodes[origin].distance == std::numeric_limits<std::int32_t>::max())
        return State6PathSearchResult{-4, 0, {}};

    State6PathSearchResult result;
    std::int32_t path_count = -1;
    std::int32_t previous_x = 0, previous_y = 0;
    auto current_index = static_cast<std::int32_t>(origin);
    bool remaining_path = false;
    while (current_index >= 0) {
        const auto& current = nodes[static_cast<std::size_t>(current_index)];
        const bool is_native_target = current.x == target_x && current.y == target_y;
        if (!native_param_7 && is_native_target && current.blocked) {
            current_index = -1;
            break;
        }
        if (path_count >= 0) {
            const auto dx = static_cast<std::int32_t>(current.x) - previous_x;
            const auto dy = static_cast<std::int32_t>(current.y) - previous_y;
            std::uint8_t direction{};
            if (dx == 1 && dy == 0) direction = 1;
            else if (dx == 1 && dy == -1) direction = 2;
            else if (dx == 0 && dy == -1) direction = 3;
            else if (dx == -1 && dy == -1) direction = 4;
            else if (dx == -1 && dy == 0) direction = 5;
            else if (dx == -1 && dy == 1) direction = 6;
            else if (dx == 0 && dy == 1) direction = 7;
            else if (dx == 1 && dy == 1) direction = 8;
            else return std::nullopt;
            result.directions.push_back(direction);
        }
        previous_x = current.x;
        previous_y = current.y;
        current_index = current.predecessor;
        ++path_count;
        if (path_count > 9) { remaining_path = current_index >= 0; break; }
        if (current_index >= 0) {
            const auto& next = nodes[static_cast<std::size_t>(current_index)];
            if (std::abs(static_cast<std::int32_t>(next.x)) > 8 ||
                std::abs(static_cast<std::int32_t>(next.y)) > 6) {
                remaining_path = true;
                break;
            }
        }
    }
    result.step_count = path_count;
    if (path_count == 0) result.native_return = 0;
    else if (remaining_path) result.native_return = 2;
    else result.native_return = 1;
    return result;
}

State6NativePathPoint transform_state6_path_target(State6PlayerPosition current,
    State6PlayerPosition target) noexcept {
    const auto floor_delta = static_cast<std::int64_t>(current.z) -
        static_cast<std::int64_t>(target.z);
    const auto x = static_cast<std::int64_t>(target.x) +
        (8 - static_cast<std::int64_t>(current.x)) - floor_delta;
    const auto y = static_cast<std::int64_t>(target.y) +
        (6 - static_cast<std::int64_t>(current.y)) - floor_delta;
    const auto z = current.z < 8
        ? (7 - static_cast<std::int64_t>(current.z)) + floor_delta
        : floor_delta + 2;
    return {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
        static_cast<std::int32_t>(z)};
}

State6PathRequestEvaluation evaluate_observed_state6_path_request(
    tibia::navigation::LocalPlayerMovementState& globals,
    const State6PathRequestEvaluationInput& input) noexcept {
    if (update_observed_state6_path_target(globals, input.current, input.request) ==
        State6PathTargetUpdate::unchanged)
        return {State6PathRequestDisposition::unchanged_target};

    const auto local = transform_state6_path_target(input.current, input.request.target);
    const bool local_target_valid = local.x >= 1 && local.x <= 15 &&
        local.y >= 1 && local.y <= 11 &&
        local.z == state6_native_path_floor_index(
            static_cast<std::uint8_t>(input.current.z)) &&
        !(local.x == 8 && local.y == 6);
    if (local_target_valid && !input.native_tile_query_result)
        return {State6PathRequestDisposition::needs_tile_validation, local};
    if (local_target_valid && input.native_tile_query_result.has_value() &&
        !*input.native_tile_query_result) {
        globals.at_0x00605A14 = 0;
        return {State6PathRequestDisposition::not_possible, local, 0, true};
    }

    if (globals.at_0x006059D4 != 0)
        return {State6PathRequestDisposition::unchanged_target, local, 0};
    if (globals.at_0x006059DC == 0)
        return {State6PathRequestDisposition::request_path_search, local};
    if (globals.at_0x006059D8 < globals.at_0x006059DC) {
        globals.at_0x006059D4 = 1;
        return {State6PathRequestDisposition::route_pending, local, 2};
    }
    return {State6PathRequestDisposition::unchanged_target, local, 0};
}

State6PathRequestEvaluation evaluate_observed_state6_path_request(
    tibia::navigation::LocalPlayerMovementState& globals,
    State6PathRequestEvaluationInput input, const tibia::map::DecodedMap& map,
    const tibia::resources::TibiaDatFile& dat) noexcept {
    const auto local = transform_state6_path_target(input.current, input.request.target);
    const bool in_native_bounds = local.x >= 1 && local.x <= 15 &&
        local.y >= 1 && local.y <= 11 &&
        local.z == state6_native_path_floor_index(
            static_cast<std::uint8_t>(input.current.z)) &&
        !(local.x == 8 && local.y == 6);
    if (in_native_bounds) {
        const auto tile_result = query_observed_state6_path_tile(map, dat, local.x,
            local.y, local.z, state6_native_path_floor_index(
                static_cast<std::uint8_t>(input.current.z)), true);
        if (tile_result == State6NativePathTileQuery::returns_zero)
            input.native_tile_query_result = false;
        else if (tile_result == State6NativePathTileQuery::returns_nonzero)
            input.native_tile_query_result = true;
        else
            input.native_tile_query_result.reset();
    }
    return evaluate_observed_state6_path_request(globals, input);
}

State6PathSearchCompletion complete_observed_state6_path_search(
    tibia::navigation::LocalPlayerMovementState& globals, std::int32_t native_result,
    std::uint32_t queued_step_count) noexcept {
    globals.at_0x006059DC = queued_step_count;
    globals.at_0x006059D4 = 0;
    const bool preserve_target = native_result == 2;
    if (!preserve_target) globals.at_0x00605A14 = 0;
    const bool begin_movement = native_result > 0;
    if (begin_movement) globals.at_0x006059D8 = 0;

    State6PathSearchError error = State6PathSearchError::none;
    switch (native_result) {
    case -1: error = State6PathSearchError::first_go_upstairs; break;
    case -2: error = State6PathSearchError::first_go_downstairs; break;
    case -3: error = State6PathSearchError::out_of_range; break;
    case -4: error = State6PathSearchError::no_route; break;
    default: break;
    }
    return {error, !preserve_target, begin_movement,
        begin_movement ? 1 : 0};
}

std::optional<State6TileNativePropertySummary> summarize_state6_tile_native_properties(
    const tibia::map::DecodedMap& map, std::uint16_t x, std::uint16_t y, std::uint8_t floor,
    const tibia::resources::TibiaDatFile& dat) noexcept {
    const auto tile = std::find_if(map.tiles.begin(), map.tiles.end(), [=](const tibia::map::Tile& value) {
        return value.x == x && value.y == y && value.floor == floor;
    });
    if (tile == map.tiles.end()) return std::nullopt;

    State6TileNativePropertySummary result{true, false, false, false, 0};
    for (const auto& object : tile->objects) {
        if (object.creature_id != 0 && object.type >= 0x61 && object.type <= 0x63) {
            result.property_0d_present = true;
            continue;
        }
        const auto* item = dat.find_item(object.type);
        if (item == nullptr) {
            result.all_objects_resolved = false;
            continue;
        }
        result.property_00_present |= item->has_native_property(0x00);
        result.property_0d_present |= item->has_native_property(0x0D);
        result.property_0f_present |= item->has_native_property(0x0F);
        if (item->has_native_property(0x1A) &&
            result.property_1a_object_count != std::numeric_limits<std::uint8_t>::max()) {
            ++result.property_1a_object_count;
        }
    }
    return result;
}

State6NativePathTileQuery query_observed_state6_path_tile(
    const tibia::map::DecodedMap& map, const tibia::resources::TibiaDatFile& dat,
    std::int32_t x, std::int32_t y, std::int32_t floor,
    std::uint8_t native_floor_index, bool check_special_floor_rules) noexcept {
    const auto find_tile = [&](std::int32_t tx, std::int32_t ty, std::int32_t tfloor)
        -> const tibia::map::Tile* {
        if (tx < 0 || ty < 0 || tfloor < 0 ||
            tx > std::numeric_limits<std::uint16_t>::max() ||
            ty > std::numeric_limits<std::uint16_t>::max() ||
            tfloor > std::numeric_limits<std::uint8_t>::max()) return nullptr;
        const auto found = std::find_if(map.tiles.begin(), map.tiles.end(),
            [&](const tibia::map::Tile& tile) {
                const auto position = tibia::map::viewport_map_position(map, tile);
                return position && position->x == tx && position->y == ty &&
                    position->floor == tfloor;
            });
        return found == map.tiles.end() ? nullptr : &*found;
    };
    const auto has_property = [&](const tibia::map::Object& object, std::uint8_t property)
        -> std::optional<bool> {
        if (object.creature_id != 0 && object.type >= 0x61 && object.type <= 0x63)
            return property == 0x0D;
        const auto* item = dat.find_item(object.type);
        if (item == nullptr) return std::nullopt;
        return item->has_native_property(property);
    };
    const auto count_property_1a = [&](std::int32_t tx, std::int32_t ty,
                                       std::int32_t tfloor) -> std::optional<std::size_t> {
        const auto* tile = find_tile(tx, ty, tfloor);
        if (tile == nullptr) return std::size_t{0};
        std::size_t count = 0;
        for (const auto& object : tile->objects) {
            const auto has = has_property(object, 0x1A);
            if (!has) return std::nullopt;
            if (*has) ++count;
        }
        return count;
    };

    if (check_special_floor_rules) {
        if (floor > 0 && x < 17 && y < 13) {
            const auto* tile = find_tile(x, y, floor);
            if (tile != nullptr && !tile->objects.empty()) {
                const auto property_00 = has_property(tile->objects.front(), 0x00);
                if (!property_00) return State6NativePathTileQuery::unresolved;
                if (!*property_00) {
                    const auto adjacent_count = count_property_1a(x + 1, y + 1, floor - 1);
                    if (!adjacent_count) return State6NativePathTileQuery::unresolved;
                    if (*adjacent_count > 2) return State6NativePathTileQuery::returns_nonzero;
                }
            }
        }
        const auto center_count = count_property_1a(8, 6, native_floor_index);
        if (!center_count) return State6NativePathTileQuery::unresolved;
        if (*center_count > 2) return State6NativePathTileQuery::returns_nonzero;
    }

    const auto* tile = find_tile(x, y, floor);
    if (tile != nullptr) {
        for (const auto& object : tile->objects) {
            const auto blocks = has_property(object, 0x0D);
            if (!blocks) return State6NativePathTileQuery::unresolved;
            if (*blocks) return State6NativePathTileQuery::returns_zero;
        }
    }
    return State6NativePathTileQuery::returns_nonzero;
}

std::optional<State6MovementCommand> make_observed_state6_movement_command(
    std::span<const std::uint8_t> directions) {
    if (directions.empty()) return std::nullopt;
    if (directions.size() == 1) {
        constexpr std::array<std::uint8_t, 9> opcodes{
            0, 0x66, 0x6A, 0x65, 0x6D, 0x68, 0x6C, 0x67, 0x6B};
        const auto direction = directions.front();
        if (direction == 0 || direction >= opcodes.size())
            throw std::invalid_argument("native movement direction must be in 1..8");
        return State6MovementCommand{opcodes[direction], {}};
    }
    if (directions.size() > 255)
        throw std::invalid_argument("native multi-step movement count must fit in one byte");
    for (const auto direction : directions)
        if (direction == 0 || direction > 8)
            throw std::invalid_argument("native movement direction must be in 1..8");
    State6MovementCommand command{0x64, {static_cast<std::uint8_t>(directions.size())}};
    command.payload.insert(command.payload.end(), directions.begin(), directions.end());
    return command;
}


}
