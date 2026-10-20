#pragma once

#include "render/software_surface.hpp"
#include "render/outfit_compositor.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <span>

namespace tibia::render {

// Places every DAT animation phase side by side, preserving the object layer order.
[[nodiscard]] SoftwareSurface compose_tibia_dat_animation(
    std::span<const std::uint8_t> tibia_spr_bytes, const resources::TibiaSprFile& tibia_spr,
    const resources::TibiaDatObject& object, std::uint16_t pattern_x = 1,
    std::uint16_t pattern_y = 1, std::uint16_t pattern_z = 1);

[[nodiscard]] SoftwareSurface compose_tibia_outfit_animation(
    std::span<const std::uint8_t> tibia_spr_bytes, const resources::TibiaSprFile& tibia_spr,
    const resources::TibiaDatObject& outfit, std::uint16_t direction,
    std::uint16_t addon_mask, std::uint16_t pattern_z, const OutfitPalette& palette);

} // namespace tibia::render
