#pragma once

#include "client/game_world_snapshot.hpp"
#include "client/inventory_layout.hpp"
#include "client/map_window_layout.hpp"
#include "render/graphics_renderer.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace tibia::client {

class InventoryPanelRenderer {
public:
    void draw(render::GraphicsRenderer& renderer,
        const GameWorldSnapshot& snapshot,
        const resources::TibiaDatFile& dat,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        const GameWindowLayout& layout,
        std::uint32_t animation_clock);

private:
    struct UploadedIcon {
        std::int32_t object_id{};
        std::uint16_t pattern_x{};
        std::uint16_t pattern_z{};
        std::uint16_t phase{};
    };
    std::array<UploadedIcon, inventory_slot_layouts.size()> uploaded_icons_{};
};

}
