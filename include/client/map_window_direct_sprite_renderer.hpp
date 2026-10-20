#pragma once

#include "client/map_window_item_frame.hpp"
#include "client/map_window_layout.hpp"
#include "render/graphics_renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>

namespace tibia::client {

class MapWindowDirectSpriteRenderer {
public:
    void invalidate() noexcept;

    [[nodiscard]] std::size_t draw(
        render::GraphicsRenderer& renderer,
        const MapWindowItemFrame& frame,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr);

    void draw_request(
        render::GraphicsRenderer& renderer,
        const map::TileDrawRequest& request,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0,
        const MapWindowLayout* viewport_clip = nullptr);

private:
    std::unordered_set<std::uint16_t> uploaded_sprite_ids_;
};

}
