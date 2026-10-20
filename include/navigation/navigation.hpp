#pragma once

#include "map/model.hpp"
#include "navigation/movement_state.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tibia::resources { struct TibiaDatFile; }

namespace tibia::network {

inline constexpr std::uint8_t state6_stop_action_opcode = 0x69;

struct State6TileNativePropertySummary {
    bool all_objects_resolved{};
    bool property_00_present{};
    bool property_0d_present{};
    bool property_0f_present{};
    std::uint8_t property_1a_object_count{};
};
[[nodiscard]] std::optional<State6TileNativePropertySummary>
summarize_state6_tile_native_properties(const tibia::map::DecodedMap& map,
    std::uint16_t x, std::uint16_t y, std::uint8_t floor,
    const tibia::resources::TibiaDatFile& dat) noexcept;
enum class State6NativePathTileQuery : std::uint8_t {
    unresolved, returns_zero, returns_nonzero
};
[[nodiscard]] State6NativePathTileQuery query_observed_state6_path_tile(
    const tibia::map::DecodedMap& map, const tibia::resources::TibiaDatFile& dat,
    std::int32_t x, std::int32_t y, std::int32_t floor,
    std::uint8_t native_floor_index, bool check_special_floor_rules) noexcept;


struct State6PlayerPosition {
    std::uint32_t x{}, y{}, z{};
    friend bool operator==(const State6PlayerPosition&, const State6PlayerPosition&) = default;
};
struct State6PathTargetRequest {
    State6PlayerPosition target{};
    std::uint8_t option_0x05A09{}, option_0x05A08{};
};
[[nodiscard]] bool state6_path_target_is_near(State6PlayerPosition current,
    State6PlayerPosition target) noexcept;
enum class State6PathTargetUpdate : std::uint8_t { unchanged, changed };
[[nodiscard]] State6PathTargetUpdate update_observed_state6_path_target(
    tibia::navigation::LocalPlayerMovementState& globals, State6PlayerPosition current_position,
    const State6PathTargetRequest& request) noexcept;
struct State6B5StallState {
    State6PlayerPosition previous_position{};
    std::uint32_t unchanged_position_calls{};
    bool previous_position_valid{};
};
struct State6B5StallEffect {
    bool stop_action_required{};
    std::uint32_t unchanged_position_calls{};
    std::uint8_t stop_action_opcode{state6_stop_action_opcode};
};
[[nodiscard]] State6B5StallEffect observe_state6_b5_position(
    State6B5StallState& state, State6PlayerPosition position) noexcept;
struct State6MovementDelta { std::int8_t x{}, y{}; };
[[nodiscard]] std::optional<State6MovementDelta>
state6_movement_delta(std::uint8_t direction) noexcept;
struct State6CreatureMoveStateInput {
    std::int32_t delta_x{}, delta_y{};
    std::int32_t duration_parameter{};
    std::int32_t creature_speed{};
    std::int32_t tick_quantum{};
    std::uint32_t creature_id{};
    std::uint32_t local_player_id{};
    std::uint32_t first_timer_sample{}, second_timer_sample{};
    std::uint8_t current_direction{};
};
struct State6CreatureMoveStateEffect {
    std::uint8_t direction{};
    std::optional<std::uint8_t> previous_direction;
    std::int32_t offset_x_subtiles{}, offset_y_subtiles{};
    std::int32_t distance_x_subtiles{}, distance_y_subtiles{};
    std::int32_t step_duration_ms{}, full_duration_ms{};
    std::uint32_t step_end_tick{}, full_end_tick{};
    bool movement_active{};
    bool local_route_index_incremented{};
    bool returned_on_matching_pending_delta{};
};
[[nodiscard]] std::optional<State6CreatureMoveStateEffect>
make_observed_state6_creature_move_state(
    tibia::navigation::LocalPlayerMovementState& globals,
    const State6CreatureMoveStateInput& input) noexcept;
[[nodiscard]] std::uint8_t state6_native_path_floor_index(std::uint8_t current_floor) noexcept;
struct State6PathRequest {
    State6PlayerPosition from{};
    State6PlayerPosition to{};
    bool allow_direct_diagonal{};
};
struct State6NativePathPoint {
    std::int32_t x{}, y{}, z{};
    friend bool operator==(const State6NativePathPoint&, const State6NativePathPoint&) = default;
};
[[nodiscard]] State6NativePathPoint transform_state6_path_target(
    State6PlayerPosition current, State6PlayerPosition target) noexcept;
struct State6PathPrecheck {
    std::optional<std::int32_t> native_return;
    std::optional<std::uint8_t> direct_direction;
    bool requires_map_search{};
};
[[nodiscard]] State6PathPrecheck
precheck_observed_state6_path_request(const State6PathRequest& request) noexcept;
struct State6PathSearchCell {
    bool blocked{};
    std::uint8_t terrain_cost{};
};
struct State6PathSearchResult {
    std::int32_t native_return{};
    std::int32_t step_count{};
    std::vector<std::uint8_t> directions;
};
[[nodiscard]] std::optional<State6PathSearchResult>
search_observed_state6_path_grid(const State6PathRequest& request,
    std::span<const State6PathSearchCell> cells, bool native_param_7);

enum class State6PathRequestDisposition : std::uint8_t {
    unchanged_target, needs_tile_validation, request_path_search, route_pending,
    not_possible
};
struct State6PathRequestEvaluation {
    State6PathRequestDisposition disposition{State6PathRequestDisposition::unchanged_target};
    State6NativePathPoint native_target{};
    std::optional<std::int32_t> native_return;
    bool show_not_possible_message{};
};
struct State6PathRequestEvaluationInput {
    State6PlayerPosition current{};
    State6PathTargetRequest request{};
    std::optional<bool> native_tile_query_result;
};
[[nodiscard]] State6PathRequestEvaluation evaluate_observed_state6_path_request(
    tibia::navigation::LocalPlayerMovementState& globals,
    const State6PathRequestEvaluationInput& input) noexcept;
[[nodiscard]] State6PathRequestEvaluation evaluate_observed_state6_path_request(
    tibia::navigation::LocalPlayerMovementState& globals,
    State6PathRequestEvaluationInput input, const tibia::map::DecodedMap& map,
    const tibia::resources::TibiaDatFile& dat) noexcept;

enum class State6PathSearchError : std::uint8_t {
    none, first_go_upstairs, first_go_downstairs, out_of_range, no_route
};
struct State6PathSearchCompletion {
    State6PathSearchError error{State6PathSearchError::none};
    bool clear_target{};
    bool begin_movement{};
    std::int32_t native_return{};
};
[[nodiscard]] State6PathSearchCompletion complete_observed_state6_path_search(
    tibia::navigation::LocalPlayerMovementState& globals, std::int32_t native_result,
    std::uint32_t queued_step_count) noexcept;

struct State6MovementCommand {
    std::uint8_t opcode{};
    std::vector<std::uint8_t> payload;
};
[[nodiscard]] std::optional<State6MovementCommand>
make_observed_state6_movement_command(std::span<const std::uint8_t> directions);


}
