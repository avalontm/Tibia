#include "render/object_compositor.hpp"

#include <stdexcept>

namespace tibia::render {

SoftwareSurface compose_tibia_dat_object(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& object,
    std::uint16_t pattern_x, std::uint16_t pattern_y, std::uint16_t pattern_z,
    std::uint16_t phase) {
    if (object.width == 0 || object.height == 0 || object.layers == 0)
        throw std::invalid_argument("Tibia.dat object has no drawable layers or tile grid");
    SoftwareSurface result(static_cast<std::uint32_t>(object.width) * 32U,
        static_cast<std::uint32_t>(object.height) * 32U, {0, 0, 0, 0});
    for (std::uint16_t layer = 1; layer <= object.layers; ++layer) {
        for (std::uint16_t y = 1; y <= object.height; ++y) {
            for (std::uint16_t x = 1; x <= object.width; ++x) {
                const auto id = object.sprite_id_at(x, y, layer, pattern_x,
                    pattern_y, pattern_z, phase);
                if (id == 0) continue;
                const auto tile = resources::decode_tibia_spr_image(tibia_spr_bytes, tibia_spr, id);
                result.draw(tile, static_cast<int>((x - 1) * 32), static_cast<int>((y - 1) * 32));
            }
        }
    }
    return result;
}

SoftwareSurface compose_tibia_dat_object_layer(std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr, const resources::TibiaDatObject& object,
    std::uint16_t layer, std::uint16_t pattern_x, std::uint16_t pattern_y,
    std::uint16_t pattern_z, std::uint16_t phase) {
    if (object.width == 0 || object.height == 0)
        throw std::invalid_argument("Tibia.dat object has no drawable tile grid");
    SoftwareSurface result(static_cast<std::uint32_t>(object.width) * 32U,
        static_cast<std::uint32_t>(object.height) * 32U, {0, 0, 0, 0});
    for (std::uint16_t y = 1; y <= object.height; ++y) {
        for (std::uint16_t x = 1; x <= object.width; ++x) {
            const auto sprite_id = object.sprite_id_at(x, y, layer, pattern_x,
                pattern_y, pattern_z, phase);
            if (sprite_id == 0) continue;
            const auto sprite = resources::decode_tibia_spr_image(tibia_spr_bytes, tibia_spr, sprite_id);
            result.draw(sprite, static_cast<int>((x - 1) * 32), static_cast<int>((y - 1) * 32));
        }
    }
    return result;
}

} // namespace tibia::render
