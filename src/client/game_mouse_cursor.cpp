#include "client/game_mouse_cursor.hpp"

#include <algorithm>
#include <stdexcept>

namespace tibia::client {
namespace {

constexpr std::uint32_t cursor_texture = 0xfb000000U;
constexpr std::uint16_t cursor_picture_id = 4;
constexpr std::uint16_t cursor_tile_x = 9;
constexpr std::uint16_t cursor_tile_y = 0;
constexpr int cursor_width = 16;
constexpr int cursor_height = 16;

resources::TibiaSprImage make_cursor_image(
    std::span<const std::uint8_t> bytes,
    const resources::TibiaPicFile& picture) {
    const auto source = resources::decode_tibia_pic_tile(bytes, picture,
        cursor_picture_id, cursor_tile_x, cursor_tile_y);
    auto image = source;
    for (int y = 0; y < cursor_height * 2; ++y) {
        for (int x = 0; x < cursor_width * 2; ++x) {
            const auto source_offset = (static_cast<std::size_t>(y / 2) * 32U +
                static_cast<std::size_t>(x / 2)) * 4U;
            const auto destination_offset = (static_cast<std::size_t>(y) * 32U +
                static_cast<std::size_t>(x)) * 4U;
            std::copy_n(source.rgba.begin() + static_cast<std::ptrdiff_t>(source_offset),
                4, image.rgba.begin() + static_cast<std::ptrdiff_t>(destination_offset));
        }
    }
    return image;
}

}

void upload_game_mouse_cursor(render::GraphicsRenderer& renderer,
    std::span<const std::uint8_t> tibia_pic_bytes,
    const resources::TibiaPicFile& tibia_pic) {
    if (tibia_pic.pictures.size() < cursor_picture_id ||
        tibia_pic.pictures[cursor_picture_id - 1].tiles_x <= cursor_tile_x ||
        tibia_pic.pictures[cursor_picture_id - 1].tiles_y <= cursor_tile_y)
        throw std::runtime_error("Tibia.pic does not contain the observed mouse cursor tile");
    renderer.upload(cursor_texture, make_cursor_image(tibia_pic_bytes, tibia_pic));
}

void draw_game_mouse_cursor(render::GraphicsRenderer& renderer,
    int x, int y, int size) {
    renderer.draw(cursor_texture, x, y, size, size);
}

}
