#include "render/pic_compositor.hpp"

#include <stdexcept>

namespace tibia::render {

SoftwareSurface compose_tibia_pic_picture(std::span<const std::uint8_t> tibia_pic_bytes,
    const resources::TibiaPicFile& tibia_pic, std::uint16_t picture_id) {
    if (picture_id == 0 || picture_id > tibia_pic.pictures.size())
        throw std::out_of_range("Tibia.pic picture ID is outside the index");
    const auto& picture = tibia_pic.pictures[picture_id - 1];
    if (picture.tiles_x == 0 || picture.tiles_y == 0)
        throw std::invalid_argument("Tibia.pic picture has an empty tile grid");
    SoftwareSurface result(static_cast<std::uint32_t>(picture.tiles_x) * 32U,
        static_cast<std::uint32_t>(picture.tiles_y) * 32U, {0, 0, 0, 0});
    for (std::uint16_t y = 0; y < picture.tiles_y; ++y) {
        for (std::uint16_t x = 0; x < picture.tiles_x; ++x) {
            const auto tile = resources::decode_tibia_pic_tile(
                tibia_pic_bytes, tibia_pic, picture_id, x, y);
            result.draw(tile, static_cast<int>(x * 32), static_cast<int>(y * 32));
        }
    }
    return result;
}

} // namespace tibia::render
