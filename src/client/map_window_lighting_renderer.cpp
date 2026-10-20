#include "client/map_window_lighting_renderer.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace tibia::client {
namespace {

constexpr std::uint32_t brightness_texture_base = 0xe8000000U;

std::uint8_t brightness_texture_index(std::uint8_t value) noexcept {
    if (value >= 1 && value <= 27) return 1;
    return static_cast<std::uint8_t>(value / 28);
}

}

void MapWindowLightingRenderer::invalidate() noexcept {
    uploaded_brightness_tiles_.fill(false);
}

std::size_t MapWindowLightingRenderer::draw_ambient(
    render::GraphicsRenderer& renderer,
    const MapWindowItemFrame& frame,
    std::span<const std::uint8_t> tibia_pic_bytes,
    const resources::TibiaPicFile& tibia_pic,
    std::uint8_t ambient_brightness,
    bool light_effects,
    const network::State6Opcode82State& world_light,
    std::uint8_t center_floor,
    const MapWindowLayout& viewport_clip,
    std::int32_t origin_x,
    std::int32_t origin_y) {
    if (frame.status == MapWindowItemFrameStatus::no_map ||
        frame.status == MapWindowItemFrameStatus::unresolved_filter_grid ||
        frame.status == MapWindowItemFrameStatus::unresolved_active_floor)
        return 0;
    const bool first_native_light_state = world_light_.global_610c00 == -1 &&
        world_light.global_610c00 != -1;
    if (!world_light_initialized_ || first_native_light_state) {
        world_light_ = world_light;
        world_light_initialized_ = true;
        last_light_animation_tick_ = frame.animation_clock;
        light_animation_tick_initialized_ = true;
    } else {
        world_light_.global_610c00 = world_light.global_610c00;
        world_light_.global_755de8 = world_light.global_755de8;
        world_light_.global_755dec = world_light.global_755dec;
        world_light_.global_755df0 = world_light.global_755df0;
    }
    if (!light_animation_tick_initialized_) {
        last_light_animation_tick_ = frame.animation_clock;
        light_animation_tick_initialized_ = true;
    } else {
        const auto current_tick = frame.animation_clock;
        const auto light_steps = current_tick >= last_light_animation_tick_
            ? current_tick / 10U - last_light_animation_tick_ / 10U
            : std::numeric_limits<std::uint32_t>::max() / 10U -
                last_light_animation_tick_ / 10U + current_tick / 10U + 1U;
        for (std::uint32_t step = 0; step < std::min(light_steps, 255U); ++step)
            network::advance_observed_state6_opcode_82(world_light_);
        last_light_animation_tick_ = current_tick;
    }
    std::array<map::RgbLight, map::light_field_size> colors{};
    std::array<std::int32_t, map::light_field_size> field_values{};
    map::LightField field(std::move(colors), std::move(field_values));
    const bool floor_below_eight = center_floor < 8;
    const map::RgbLight source{{
        static_cast<std::uint8_t>(std::clamp(world_light_.global_755df4, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(world_light_.global_755df8, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(world_light_.global_755dfc, 0, 255))}};
    const auto source_strength = static_cast<std::uint8_t>(
        std::clamp(world_light_.global_610bfc, 0, 255));
    for (std::int32_t x = 0; x < static_cast<std::int32_t>(map::light_field_width); ++x)
        for (std::int32_t y = 0; y < static_cast<std::int32_t>(map::light_field_height); ++y)
            field.blend(x, y, source_strength, source, ambient_brightness, floor_below_eight);
    const auto center_render_floor = center_floor < 8
        ? 7 - static_cast<std::int32_t>(center_floor) : 2;
    const auto adjusted_floor = static_cast<std::int32_t>(center_floor) - 8 +
        center_render_floor;
    if (adjusted_floor >= 0 && adjusted_floor < 7 &&
        adjusted_floor <= frame.highest_active_floor) {
        const auto adjusted_strength = source_strength == 0
            ? 0 : static_cast<std::uint8_t>(source_strength - 1);
        const auto& property_18_cells =
            frame.floor_light_property_18_masks[static_cast<std::size_t>(adjusted_floor + 1)];
        for (std::int32_t x = 1; x < static_cast<std::int32_t>(map::light_field_width); ++x) {
            for (std::int32_t y = 1; y < static_cast<std::int32_t>(map::light_field_height); ++y) {
                const auto source_cell = static_cast<std::size_t>(x - 1) *
                    map::light_field_height + static_cast<std::size_t>(y - 1);
                if (property_18_cells[source_cell] == 0) continue;
                field.blend(x, y, adjusted_strength, source,
                    ambient_brightness, floor_below_eight);
            }
        }
    }
    for (const auto& light_source : frame.light_sources)
        if (light_source.floor < map::light_field_floor_count)
            field.add_source(light_source, frame.floor_light_property_masks[light_source.floor]);
    light_field_ = field;

    if (light_effects && renderer.supports_light_quads()) {
        std::array<render::LightQuad, 19 * 15> quads{};
        std::size_t quad_index{};
        for (std::int32_t y = 0; y < 15; ++y) {
            for (std::int32_t x = 0; x < 19; ++x) {
                const auto color = [&](std::int32_t cell_x, std::int32_t cell_y) {
                    const auto value = field.clamped_color(cell_x, cell_y);
                    return render::RgbColor{value.channels[0], value.channels[1], value.channels[2]};
                };
                quads[quad_index++] = {
                    origin_x + x * map::map_tile_pixels - frame.camera_offset_x - 16,
                    origin_y + y * map::map_tile_pixels - frame.camera_offset_y - 16,
                    map::map_tile_pixels,
                    map::map_tile_pixels,
                    {color(x - 1, y - 1), color(x, y - 1),
                        color(x - 1, y), color(x, y)}};
            }
        }
        renderer.draw_light_quads(quads, viewport_clip.x, viewport_clip.y,
            viewport_clip.width, viewport_clip.height);
        return quads.size();
    }

    std::size_t draw_count{};
    for (std::int32_t x = 0; x < static_cast<std::int32_t>(map::light_field_width); ++x) {
        for (std::int32_t y = 0; y < static_cast<std::int32_t>(map::light_field_height); ++y) {
            const auto index = brightness_texture_index(field.average_color(x, y));
            if (index > 7) continue;
            if (!uploaded_brightness_tiles_[index]) {
                renderer.upload(brightness_texture_base + index,
                    resources::decode_tibia_pic_tile(tibia_pic_bytes, tibia_pic,
                        7, index, 0));
                uploaded_brightness_tiles_[index] = true;
            }
            const auto destination_x = origin_x + x * map::map_tile_pixels - frame.camera_offset_x;
            const auto destination_y = origin_y + y * map::map_tile_pixels - frame.camera_offset_y;
            renderer.draw_clipped(brightness_texture_base + index,
                destination_x, destination_y, map::map_tile_pixels, map::map_tile_pixels,
                viewport_clip.x, viewport_clip.y, viewport_clip.width, viewport_clip.height);
            ++draw_count;
        }
    }
    return draw_count;
}

std::uint8_t MapWindowLightingRenderer::average_light(std::int32_t local_x,
    std::int32_t local_y) const noexcept {
    return light_field_ ? light_field_->average_color(local_x, local_y) : 0;
}

}
