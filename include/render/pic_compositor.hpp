#pragma once

#include "render/software_surface.hpp"
#include "resources/tibia_pic.hpp"

#include <span>

namespace tibia::render {

// Rebuilds a complete Tibia.pic picture from its row-major 32x32 tiles.
[[nodiscard]] SoftwareSurface compose_tibia_pic_picture(
    std::span<const std::uint8_t> tibia_pic_bytes, const resources::TibiaPicFile& tibia_pic,
    std::uint16_t picture_id);

} // namespace tibia::render
