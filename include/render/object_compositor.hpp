#pragma once

#include "render/software_surface.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <cstdint>
#include <span>

namespace tibia::render {

[[nodiscard]] SoftwareSurface compose_tibia_dat_object(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const resources::TibiaDatObject& object,
    std::uint16_t pattern_x = 1,
    std::uint16_t pattern_y = 1,
    std::uint16_t pattern_z = 1,
    std::uint16_t phase = 1);

// Decodes one complete tile-grid layer for a selected Tibia.dat object variant.
[[nodiscard]] SoftwareSurface compose_tibia_dat_object_layer(
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const resources::TibiaDatObject& object,
    std::uint16_t layer,
    std::uint16_t pattern_x = 1,
    std::uint16_t pattern_y = 1,
    std::uint16_t pattern_z = 1,
    std::uint16_t phase = 1);

} // namespace tibia::render
