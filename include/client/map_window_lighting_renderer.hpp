#pragma once

#include "client/map_window_item_frame.hpp"
#include "client/map_window_layout.hpp"
#include "map/light_field.hpp"
#include "render/graphics_renderer.hpp"
#include "resources/tibia_pic.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tibia::client {

class MapWindowLightingRenderer {
public:
    void invalidate() noexcept;
    [[nodiscard]] std::size_t draw_ambient(
        render::GraphicsRenderer& renderer,
        const MapWindowItemFrame& frame,
        std::span<const std::uint8_t> tibia_pic_bytes,
        const resources::TibiaPicFile& tibia_pic,
        std::uint8_t ambient_brightness,
        bool light_effects,
        const network::State6Opcode82State& world_light,
        std::uint8_t center_floor,
        const MapWindowLayout& viewport_clip,
        std::int32_t origin_x = 0,
        std::int32_t origin_y = 0);
    [[nodiscard]] std::uint8_t average_light(std::int32_t local_x,
        std::int32_t local_y) const noexcept;

private:
    std::array<bool, 8> uploaded_brightness_tiles_{};
    network::State6Opcode82State world_light_;
    bool world_light_initialized_{};
    std::uint32_t last_light_animation_tick_{};
    bool light_animation_tick_initialized_{};
    std::optional<map::LightField> light_field_;
};

}
