#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/map_window_item_frame.hpp"
#include "client/map_window_layout.hpp"
#include "client/creature_render_batch.hpp"
#include "render/graphics_renderer.hpp"
#include "resources/tibia_dat.hpp"
#include "resources/tibia_spr.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tibia::client {

struct CreatureInfoOverlay {
    std::string name;
    std::int32_t name_x{};
    std::int32_t name_y{};
    std::int32_t bar_x{};
    std::int32_t bar_y{};
    std::int32_t bar_fill_width{};
    std::array<std::uint8_t, 3> color{};
    std::uint16_t primary_mark_skin{};
    std::int32_t primary_mark_x{};
    std::int32_t primary_mark_y{};
    std::uint16_t secondary_mark_skin{};
    std::int32_t secondary_mark_x{};
    std::int32_t secondary_mark_y{};
    bool draw_primary_mark{};
    bool draw_secondary_mark{};
};

class MapWindowCreatureRenderer {
public:
    void prepare_frame(
        const GameWorldSnapshot& snapshot,
        const MapWindowItemFrame& map_frame,
        const resources::TibiaDatFile& dat,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        std::uint32_t animation_clock,
        std::uint32_t elapsed_since_animation_epoch);

    [[nodiscard]] std::size_t draw_tile(
        render::GraphicsRenderer& renderer,
        const MapWindowItemFrame& map_frame,
        const MapWindowLayout& viewport_clip,
        std::uint8_t floor,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0);

    [[nodiscard]] std::vector<CreatureInfoOverlay> prepare_info_overlays(
        const GameWorldSnapshot& snapshot,
        const resources::TibiaDatFile& dat,
        std::int32_t creature_info_mode,
        std::int32_t viewport_width,
        std::int32_t viewport_height,
        std::int32_t font_height,
        std::int32_t camera_offset_x,
        std::int32_t camera_offset_y,
        const std::function<std::int32_t(std::string_view)>& text_width,
        const std::function<std::uint8_t(std::int32_t, std::int32_t)>&
            light_at) const;

private:
    std::vector<CreatureRenderBatch> floor_batches_;
    std::uint32_t last_mark_blink_tick_{};
    bool mark_blink_visible_{};
};

}
