#pragma once

#include "render/software_surface.hpp"
#include "resources/tibia_dat.hpp"

#include <array>
#include <span>

namespace tibia::render {

using OutfitPalette = std::array<resources::TibiaSprColor, 4>;

struct OutfitRenderOptions {
    std::uint16_t direction{1}; // pattern_x is the four-way direction selector.
    std::uint16_t addon_mask{}; // Bit 0 selects pattern_y 2, bit 1 selects pattern_y 3, etc.
    std::uint16_t pattern_z{1};
    std::uint16_t phase{1};
};

// Converts the client's 0..132 outfit color selector into its observed RGB value.
[[nodiscard]] resources::TibiaSprColor tibia_outfit_color(std::uint16_t selector);

// Applies the four mask labels used by the 7.92 renderer. Label order in the
// mask is yellow, red, green, blue; each palette entry is an RGB outfit color.
[[nodiscard]] resources::TibiaSprImage colorize_tibia_outfit_tile(
    const resources::TibiaSprImage& base, const resources::TibiaSprImage& mask,
    const OutfitPalette& palette);

[[nodiscard]] SoftwareSurface compose_tibia_outfit(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& outfit,
    const OutfitRenderOptions& options, const OutfitPalette& palette);

} // namespace tibia::render
