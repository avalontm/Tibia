#pragma once

#include "client/container_panel_layout.hpp"
#include "client/game_world_snapshot.hpp"
#include "render/graphics_renderer.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>

namespace tibia::client {

class ContainerPanelRenderer {
public:
    void draw(render::GraphicsRenderer& renderer,
        const GameWorldSnapshot& snapshot,
        const resources::TibiaDatFile& dat,
        std::span<const std::uint8_t> tibia_spr_bytes,
        const resources::TibiaSprFile& tibia_spr,
        const std::vector<ContainerPanelLayout>& panels,
        std::uint32_t animation_clock);

private:
    struct UploadedIcon {
        std::int32_t object_id{};
        std::uint16_t pattern_x{};
        std::uint16_t pattern_z{};
        std::uint16_t phase{};
    };
    std::unordered_map<std::uint32_t, UploadedIcon> uploaded_icons_;
};

}
