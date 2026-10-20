#include "client/creature_outfit_appearance.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace tibia::client {
namespace {

std::optional<std::uint8_t> native_u8(
    const network::State6CreatureRuntimeState& creature, std::uint16_t offset) noexcept {
    if (offset < network::State6CreatureRuntimeState::first_native_offset)
        return std::nullopt;
    const auto index = static_cast<std::size_t>(
        offset - network::State6CreatureRuntimeState::first_native_offset);
    if (index >= creature.native_fields.size()) return std::nullopt;
    return creature.native_fields[index];
}

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

std::uint32_t permitted_addon_mask(std::uint8_t pattern_y) noexcept {
    if (pattern_y == 2) return 1;
    if (pattern_y == 3) return 3;
    if (pattern_y == 4) return 7;
    return 0;
}

}

std::optional<CreatureOutfitAppearance> resolve_creature_outfit_appearance(
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat) {
    const auto outfit_value = native_u32(creature, 0x60);
    if (!outfit_value || *outfit_value == 0 ||
        *outfit_value > std::numeric_limits<std::uint16_t>::max())
        return std::nullopt;

    const auto outfit_type = static_cast<std::uint16_t>(*outfit_value);
    const auto* outfit = dat.outfits.find(outfit_type);
    if (outfit == nullptr) return std::nullopt;

    const auto addon_value = native_u32(creature, 0x74);
    const std::array<std::uint16_t, 4> color_offsets{0x64, 0x68, 0x6c, 0x70};
    if (!addon_value) return std::nullopt;

    render::OutfitPalette palette{};
    for (std::size_t i = 0; i < color_offsets.size(); ++i) {
        const auto selector = native_u32(creature, color_offsets[i]);
        if (!selector || *selector > std::numeric_limits<std::uint16_t>::max())
            return std::nullopt;
        palette[i] = render::tibia_outfit_color(static_cast<std::uint16_t>(*selector));
    }

    return CreatureOutfitAppearance{
        outfit,
        outfit_type,
        *addon_value & permitted_addon_mask(outfit->pattern_y),
        palette
    };
}

std::optional<std::uint16_t> resolve_creature_outfit_phase(
    const resources::TibiaDatObject& outfit,
    const CreatureOutfitPhaseContext& context) noexcept {
    const auto phase_count = static_cast<std::int32_t>(outfit.animation_frames);
    if (phase_count < 1) return std::nullopt;
    if (phase_count == 1) return 1;

    if (outfit.has_native_property(0x1c)) {
        return static_cast<std::uint16_t>(
            (context.animation_clock / 3U) % static_cast<std::uint32_t>(phase_count) + 1U);
    }

    const auto elapsed_tick = std::bit_cast<std::int32_t>(
        context.elapsed_since_animation_epoch);
    if (context.movement_offset_x == 0 && context.movement_offset_y == 0 &&
        context.previous_movement_tick < elapsed_tick)
        return 1;

    const auto distance_x = context.movement_offset_x < 0
        ? -static_cast<std::int64_t>(context.movement_offset_x)
        : static_cast<std::int64_t>(context.movement_offset_x);
    const auto distance_y = context.movement_offset_y < 0
        ? -static_cast<std::int64_t>(context.movement_offset_y)
        : static_cast<std::int64_t>(context.movement_offset_y);
    const auto phase_distance = std::max(distance_x, distance_y);
    const auto phase = (phase_distance / 8) % (phase_count - 1) + 2;
    return static_cast<std::uint16_t>(phase);
}

std::optional<std::uint16_t> resolve_creature_outfit_phase_for_state(
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatObject& outfit,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) noexcept {
    const auto movement_x = native_u32(creature, 0x30);
    const auto movement_y = native_u32(creature, 0x34);
    const auto previous_movement_tick = native_u32(creature, 0x38);
    if (!movement_x || !movement_y || !previous_movement_tick) return std::nullopt;

    const CreatureOutfitPhaseContext context{
        animation_clock,
        elapsed_since_animation_epoch,
        std::bit_cast<std::int32_t>(*movement_x),
        std::bit_cast<std::int32_t>(*movement_y),
        std::bit_cast<std::int32_t>(*previous_movement_tick)
    };
    return resolve_creature_outfit_phase(outfit, context);
}

std::optional<std::uint16_t> resolve_creature_outfit_direction_for_state(
    const network::State6CreatureRuntimeState& creature,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state) noexcept {
    std::uint32_t direction{};
    if (creature.creature_id == local_player_id &&
        (movement_state.at_0x00605A20 != 0 || movement_state.at_0x00605A1C != 0)) {
        direction = movement_state.at_0x00605A18;
    } else {
        const auto moving = native_u8(creature, 0x4c);
        if (!moving) return std::nullopt;
        const auto direction_value = native_u32(creature, *moving != 0 ? 0x54 : 0x50);
        if (!direction_value) return std::nullopt;
        direction = *direction_value;
    }
    if (direction >= std::numeric_limits<std::uint16_t>::max()) return std::nullopt;
    return static_cast<std::uint16_t>(direction + 1U);
}

render::SoftwareSurface compose_creature_outfit_appearance(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const CreatureOutfitAppearance& appearance,
    std::uint16_t direction, std::uint16_t pattern_z, std::uint16_t phase) {
    if (appearance.outfit == nullptr)
        throw std::invalid_argument("creature outfit appearance has no DAT object");
    render::OutfitRenderOptions options;
    options.direction = direction;
    options.addon_mask = static_cast<std::uint16_t>(appearance.addon_mask);
    options.pattern_z = pattern_z;
    options.phase = phase;
    return render::compose_tibia_outfit(tibia_spr_bytes, tibia_spr,
        *appearance.outfit, options, appearance.palette);
}

std::optional<render::SoftwareSurface> compose_creature_outfit_from_state(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) {
    const auto appearance = resolve_creature_outfit_appearance(creature, dat);
    if (!appearance) return std::nullopt;
    const auto direction = resolve_creature_outfit_direction_for_state(
        creature, local_player_id, movement_state);
    if (!direction) return std::nullopt;
    const auto phase = resolve_creature_outfit_phase_for_state(
        creature, *appearance->outfit, animation_clock, elapsed_since_animation_epoch);
    if (!phase) return std::nullopt;
    return compose_creature_outfit_appearance(tibia_spr_bytes, tibia_spr,
        *appearance, *direction, 1, *phase);
}

}
