#pragma once

#include "network/message_dispatcher.hpp"
#include "navigation/movement_state.hpp"
#include "render/outfit_compositor.hpp"
#include "render/software_surface.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tibia::client {

struct CreatureOutfitAppearance {
    const resources::TibiaDatObject* outfit{};
    std::uint16_t outfit_type{};
    std::uint32_t addon_mask{};
    render::OutfitPalette palette{};
};

struct CreatureOutfitPhaseContext {
    std::uint32_t animation_clock{};
    std::uint32_t elapsed_since_animation_epoch{};
    std::int32_t movement_offset_x{};
    std::int32_t movement_offset_y{};
    std::int32_t previous_movement_tick{};
};

[[nodiscard]] std::optional<CreatureOutfitAppearance> resolve_creature_outfit_appearance(
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat);

[[nodiscard]] std::optional<std::uint16_t> resolve_creature_outfit_phase(
    const resources::TibiaDatObject& outfit,
    const CreatureOutfitPhaseContext& context) noexcept;

[[nodiscard]] std::optional<std::uint16_t> resolve_creature_outfit_phase_for_state(
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatObject& outfit,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) noexcept;

[[nodiscard]] std::optional<std::uint16_t> resolve_creature_outfit_direction_for_state(
    const network::State6CreatureRuntimeState& creature,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state) noexcept;

[[nodiscard]] render::SoftwareSurface compose_creature_outfit_appearance(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const CreatureOutfitAppearance& appearance,
    std::uint16_t direction, std::uint16_t pattern_z, std::uint16_t phase);

[[nodiscard]] std::optional<render::SoftwareSurface> compose_creature_outfit_from_state(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch);

}
