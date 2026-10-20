#pragma once

#include "client/map_window_direct_sprite_renderer.hpp"
#include "client/map_window_layout.hpp"
#include "resources/tibia_pic.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_set>
#include <vector>

namespace tibia::client {

struct MapWindowTileSurfaceDrawStats {
    std::size_t draw_calls{};
    std::size_t visible_cells_with_pixels{};
    std::size_t nontransparent_pixels{};
};

class MapWindowTileSurfaceRenderer {
public:
    MapWindowTileSurfaceRenderer();

    void invalidate() noexcept;

    [[nodiscard]] MapWindowTileSurfaceDrawStats draw(
        render::GraphicsRenderer& renderer,
        const MapWindowItemFrame& frame,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        std::span<const std::uint8_t> tibia_pic_bytes,
        const resources::TibiaPicFile& tibia_pic,
        const MapWindowLayout& viewport_clip,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0,
        const std::function<void(std::int32_t)>& after_floor = {},
        const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& during_tile_pass = {},
        const std::function<void()>& before_overlays = {},
        const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& after_tile = {});

private:
    MapWindowDirectSpriteRenderer direct_sprites_;
    std::vector<render::SoftwareSurface> cells_;
    std::array<bool, map::map_cache_columns * map::map_cache_rows> cell_texture_uploaded_{};
    std::array<bool, map::map_cache_columns * map::map_cache_rows> cell_texture_dirty_{};
    std::unordered_set<std::uint16_t> uploaded_picture_ids_;
};

}
