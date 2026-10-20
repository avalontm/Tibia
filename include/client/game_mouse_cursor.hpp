#pragma once

#include "resources/tibia_pic.hpp"
#include "render/graphics_renderer.hpp"

#include <cstdint>
#include <span>

namespace tibia::client {

void upload_game_mouse_cursor(render::GraphicsRenderer& renderer,
    std::span<const std::uint8_t> tibia_pic_bytes,
    const resources::TibiaPicFile& tibia_pic);

void draw_game_mouse_cursor(render::GraphicsRenderer& renderer,
    int x, int y, int size);

}
