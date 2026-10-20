#pragma once

#include "navigation/movement_state.hpp"
#include "network/message_dispatcher.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace tibia::client {

[[nodiscard]] std::optional<resources::TibiaSprImage> compose_battle_creature_icon(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch);

}
