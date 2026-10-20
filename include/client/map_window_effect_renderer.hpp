#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_layout.hpp"
#include "render/graphics_renderer.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace tibia::client {

class MapWindowEffectRenderer {
public:
    using TextDraw = std::function<void(std::string_view, int, int, std::uint8_t)>;

    [[nodiscard]] std::size_t draw_tile(
        render::GraphicsRenderer& renderer,
        const GameWorldSnapshot& snapshot,
        const resources::TibiaDatFile& dat,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        const MapWindowLayout& viewport_clip,
        std::uint8_t floor,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t camera_offset_x,
        std::int32_t camera_offset_y,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0);

    [[nodiscard]] std::size_t collect_floor_text_effects(
        const GameWorldSnapshot& snapshot,
        std::uint8_t floor,
        std::int32_t camera_offset_x,
        std::int32_t camera_offset_y,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0,
        const TextDraw& draw_text = {});

    [[nodiscard]] std::size_t draw_deferred_projectiles(
        render::GraphicsRenderer& renderer,
        const GameWorldSnapshot& snapshot,
        const resources::TibiaDatFile& dat,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        const MapWindowLayout& viewport_clip,
        std::int32_t highest_active_floor,
        std::int32_t camera_offset_x,
        std::int32_t camera_offset_y,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0);
};

}
