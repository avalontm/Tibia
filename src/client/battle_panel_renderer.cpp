#include "client/battle_panel_renderer.hpp"

#include "client/creature_outfit_appearance.hpp"

#include <algorithm>
#include <cstring>

namespace tibia::client {

std::optional<resources::TibiaSprImage> compose_battle_creature_icon(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const network::State6CreatureRuntimeState& creature,
    const resources::TibiaDatFile& dat,
    std::uint32_t local_player_id,
    const navigation::LocalPlayerMovementState& movement_state,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) {
    const auto surface = compose_creature_outfit_from_state(
        tibia_spr_bytes, tibia_spr, creature, dat, local_player_id,
        movement_state, animation_clock, elapsed_since_animation_epoch);
    if (!surface || surface->width() == 0 || surface->height() == 0)
        return std::nullopt;

    resources::TibiaSprImage image;
    const auto width = std::min<std::uint32_t>(
        surface->width(), static_cast<std::uint32_t>(image.width));
    const auto height = std::min<std::uint32_t>(
        surface->height(), static_cast<std::uint32_t>(image.height));
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto source_offset = static_cast<std::size_t>(y) *
            surface->width() * 4U;
        const auto destination_offset = static_cast<std::size_t>(y) *
            image.width * 4U;
        std::memcpy(image.rgba.data() + destination_offset,
            surface->pixels().data() + source_offset,
            static_cast<std::size_t>(width) * 4U);
    }
    return image;
}

}
