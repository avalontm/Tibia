#include "render/animation_compositor.hpp"

#include "render/object_compositor.hpp"

#include <limits>
#include <stdexcept>

namespace tibia::render {

SoftwareSurface compose_tibia_dat_animation(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& object,
    std::uint16_t pattern_x, std::uint16_t pattern_y, std::uint16_t pattern_z) {
    if (object.animation_frames == 0)
        throw std::invalid_argument("Tibia.dat object has no animation phases");
    const auto frame_width = static_cast<std::uint64_t>(object.width) * 32U;
    const auto frame_height = static_cast<std::uint64_t>(object.height) * 32U;
    const auto strip_width = frame_width * object.animation_frames;
    if (strip_width > std::numeric_limits<std::uint32_t>::max() ||
        frame_height > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Tibia.dat animation strip dimensions overflow");
    SoftwareSurface result(static_cast<std::uint32_t>(strip_width),
        static_cast<std::uint32_t>(frame_height), {0, 0, 0, 0});
    for (std::uint16_t phase = 1; phase <= object.animation_frames; ++phase) {
        const auto frame = compose_tibia_dat_object(tibia_spr_bytes, tibia_spr, object,
            pattern_x, pattern_y, pattern_z, phase);
        result.draw(frame, static_cast<int>((phase - 1) * frame_width), 0);
    }
    return result;
}

SoftwareSurface compose_tibia_outfit_animation(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& outfit,
    std::uint16_t direction, std::uint16_t addon_mask, std::uint16_t pattern_z,
    const OutfitPalette& palette) {
    if (outfit.animation_frames == 0)
        throw std::invalid_argument("Tibia.dat outfit has no animation phases");
    const auto frame_width = static_cast<std::uint64_t>(outfit.width) * 32U;
    const auto frame_height = static_cast<std::uint64_t>(outfit.height) * 32U;
    const auto strip_width = frame_width * outfit.animation_frames;
    if (strip_width > std::numeric_limits<std::uint32_t>::max() ||
        frame_height > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Tibia.dat outfit animation strip dimensions overflow");
    SoftwareSurface result(static_cast<std::uint32_t>(strip_width),
        static_cast<std::uint32_t>(frame_height), {0, 0, 0, 0});
    for (std::uint16_t phase = 1; phase <= outfit.animation_frames; ++phase) {
        OutfitRenderOptions options;
        options.direction = direction;
        options.addon_mask = addon_mask;
        options.pattern_z = pattern_z;
        options.phase = phase;
        const auto frame = compose_tibia_outfit(tibia_spr_bytes, tibia_spr, outfit, options, palette);
        result.draw(frame, static_cast<int>((phase - 1) * frame_width), 0);
    }
    return result;
}

} // namespace tibia::render
