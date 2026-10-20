#include "render/creature_projection.hpp"

#include <bit>

namespace tibia::render {
namespace {

std::optional<std::uint32_t> native_u32(
    const network::State6CreatureRuntimeState& creature, std::uint16_t offset) noexcept {
    if (offset < network::State6CreatureRuntimeState::first_native_offset)
        return std::nullopt;
    const auto index = static_cast<std::size_t>(
        offset - network::State6CreatureRuntimeState::first_native_offset);
    if (index + 4 > creature.native_fields.size()) return std::nullopt;
    const auto& fields = creature.native_fields;
    return static_cast<std::uint32_t>(fields[index]) |
        (static_cast<std::uint32_t>(fields[index + 1]) << 8U) |
        (static_cast<std::uint32_t>(fields[index + 2]) << 16U) |
        (static_cast<std::uint32_t>(fields[index + 3]) << 24U);
}

std::int32_t add_wrapped_movement(std::uint32_t creature_offset,
    std::int32_t local_player_offset) noexcept {
    const auto value = creature_offset + static_cast<std::uint32_t>(local_player_offset) * 32U;
    return std::bit_cast<std::int32_t>(value);
}

}

std::optional<CreatureMovementOffset> resolve_creature_movement_offset(
    const network::State6CreatureRuntimeState& creature,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state) noexcept {
    const auto x = native_u32(creature, 0x30);
    const auto y = native_u32(creature, 0x34);
    if (!x || !y) return std::nullopt;
    auto movement_x = std::bit_cast<std::int32_t>(*x);
    auto movement_y = std::bit_cast<std::int32_t>(*y);
    if (creature.creature_id == local_player_id) {
        movement_x = add_wrapped_movement(*x, movement_state.at_0x00605A20);
        movement_y = add_wrapped_movement(*y, movement_state.at_0x00605A1C);
    }
    return CreatureMovementOffset{movement_x, movement_y};
}

std::optional<CreatureScreenPosition> project_creature_screen_position(
    std::int32_t tile_x, std::int32_t tile_y,
    std::int32_t reference_tile_x, std::int32_t reference_tile_y,
    CreatureMovementOffset movement_offset,
    std::int32_t outfit_extent_x, std::int32_t outfit_extent_y) noexcept {
    if (tile_x < 0 || tile_x >= map::map_viewport_tiles_x ||
        tile_y < 0 || tile_y >= map::map_viewport_tiles_y)
        return std::nullopt;
    const auto x = static_cast<std::int64_t>(movement_offset.x) +
        (static_cast<std::int64_t>(tile_x) - reference_tile_x) * map::map_tile_pixels -
        outfit_extent_x;
    const auto y = static_cast<std::int64_t>(movement_offset.y) +
        (static_cast<std::int64_t>(tile_y) - reference_tile_y) * map::map_tile_pixels -
        outfit_extent_y;
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX)
        return std::nullopt;
    return CreatureScreenPosition{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

}
