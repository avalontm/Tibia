#include "client/map_window_creature_renderer.hpp"

#include "client/creature_render_plan.hpp"
#include "client/ui_skins_792.hpp"
#include "map/model.hpp"
#include "map/tile_object_order.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace tibia::client {
namespace {

constexpr std::uint32_t creature_texture_base = 0xe4000000U;

resources::TibiaSprImage to_image(const render::SoftwareSurface& surface,
    std::uint32_t left, std::uint32_t top) {
    resources::TibiaSprImage image;
    const auto right = std::min(surface.width(), left + 32U);
    const auto bottom = std::min(surface.height(), top + 32U);
    for (auto y = top; y < bottom; ++y) {
        const auto source_offset = (static_cast<std::size_t>(y) * surface.width() + left) * 4U;
        const auto destination_offset = static_cast<std::size_t>(y - top) * 32U * 4U;
        const auto row_bytes = static_cast<std::size_t>(right - left) * 4U;
        std::memcpy(image.rgba.data() + destination_offset,
            surface.pixels().data() + source_offset, row_bytes);
    }
    return image;
}

bool has_pixels(const resources::TibiaSprImage& image) noexcept {
    for (std::size_t i = 3; i < image.rgba.size(); i += 4)
        if (image.rgba[i] != 0) return true;
    return false;
}

}

void MapWindowCreatureRenderer::prepare_frame(
    const GameWorldSnapshot& snapshot,
    const MapWindowItemFrame& map_frame,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::uint32_t animation_clock,
    std::uint32_t elapsed_since_animation_epoch) {
    floor_batches_.clear();
    if (elapsed_since_animation_epoch - last_mark_blink_tick_ > 300U) {
        mark_blink_visible_ = !mark_blink_visible_;
        last_mark_blink_tick_ = elapsed_since_animation_epoch;
    }
    if (map_frame.highest_active_floor < 0) return;
    floor_batches_.resize(static_cast<std::size_t>(map_frame.highest_active_floor) + 1U);
    for (std::int32_t floor = 0; floor <= map_frame.highest_active_floor; ++floor) {
        const auto plan = prepare_creature_render_plan(
            snapshot, dat, static_cast<std::uint8_t>(floor), 0, 0);
        floor_batches_[static_cast<std::size_t>(floor)] = compose_creature_render_batch(
            snapshot, plan, dat, tibia_spr_bytes, tibia_spr,
            animation_clock, elapsed_since_animation_epoch);
    }
}

std::size_t MapWindowCreatureRenderer::draw_tile(
    render::GraphicsRenderer& renderer,
    const MapWindowItemFrame& map_frame,
    const MapWindowLayout& viewport_clip,
    std::uint8_t floor,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t origin_x,
    std::int32_t origin_y) {
    if (viewport_clip.width <= 0 || viewport_clip.height <= 0 ||
        local_x < 0 || local_x >= static_cast<std::int32_t>(map::map_viewport_tiles_x) ||
        local_y < 0 || local_y >= static_cast<std::int32_t>(map::map_viewport_tiles_y) ||
        floor >= floor_batches_.size())
        return 0;

    const auto& batch = floor_batches_[floor];
    const auto texture_id = creature_texture_base +
        static_cast<std::uint32_t>(local_x * map::map_viewport_tiles_y + local_y);
    const auto clip_x = viewport_clip.x + origin_x;
    const auto clip_y = viewport_clip.y + origin_y;
    std::size_t drawn{};

    for (const auto& creature : batch.entries) {
        if (creature.local_x != local_x || creature.local_y != local_y ||
            creature.surface.width() == 0 || creature.surface.height() == 0)
            continue;

        const auto sprite_x = static_cast<std::int64_t>(creature.sort_x) -
            (static_cast<std::int64_t>(creature.surface.width()) - 32) -
            map_frame.camera_offset_x;
        const auto sprite_y = static_cast<std::int64_t>(creature.sort_y) -
            (static_cast<std::int64_t>(creature.surface.height()) - 32) -
            map_frame.camera_offset_y;
        if (sprite_x < std::numeric_limits<std::int32_t>::min() ||
            sprite_x > std::numeric_limits<std::int32_t>::max() ||
            sprite_y < std::numeric_limits<std::int32_t>::min() ||
            sprite_y > std::numeric_limits<std::int32_t>::max())
            continue;

        bool submitted = false;
        for (std::uint32_t top = 0; top < creature.surface.height(); top += 32U) {
            for (std::uint32_t left = 0; left < creature.surface.width(); left += 32U) {
                auto image = to_image(creature.surface, left, top);
                if (!has_pixels(image)) continue;
                const auto destination_x = static_cast<std::int64_t>(viewport_clip.x) +
                    origin_x + sprite_x + left;
                const auto destination_y = static_cast<std::int64_t>(viewport_clip.y) +
                    origin_y + sprite_y + top;
                if (destination_x < std::numeric_limits<std::int32_t>::min() ||
                    destination_x > std::numeric_limits<std::int32_t>::max() ||
                    destination_y < std::numeric_limits<std::int32_t>::min() ||
                    destination_y > std::numeric_limits<std::int32_t>::max())
                    continue;
                renderer.upload(texture_id, image);
                renderer.draw_clipped(texture_id,
                    static_cast<std::int32_t>(destination_x),
                    static_cast<std::int32_t>(destination_y), 32, 32,
                    clip_x, clip_y, viewport_clip.width, viewport_clip.height);
                submitted = true;
            }
        }
        if (submitted) ++drawn;
    }
    return drawn;
}

std::vector<CreatureInfoOverlay> MapWindowCreatureRenderer::prepare_info_overlays(
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::int32_t creature_info_mode,
    std::int32_t viewport_width,
    std::int32_t viewport_height,
    std::int32_t font_height,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    const std::function<std::int32_t(std::string_view)>& text_width,
    const std::function<std::uint8_t(std::int32_t, std::int32_t)>& light_at) const {
    std::vector<CreatureInfoOverlay> overlays;
    if (creature_info_mode == 0 || !snapshot.map || viewport_width < 29 ||
        viewport_height < 7 || !text_width || !light_at)
        return overlays;
    std::uint8_t local_party_shield{};
    for (const auto& candidate : snapshot.creatures) {
        if (candidate.state.creature_id == snapshot.map->local_player_id) {
            local_party_shield = candidate.state.native_fields[0x68];
            break;
        }
    }

    for (std::size_t floor = 0; floor < floor_batches_.size(); ++floor) {
        for (const auto& creature : floor_batches_[floor].entries) {
            auto bar_x = std::clamp(creature.sort_x - 14 - camera_offset_x,
                1, viewport_width - 28);
            auto bar_y = creature.sort_y - 4 - camera_offset_y;
            const bool show_name = creature_info_mode == 2;
            const auto name_width = show_name
                ? std::max(0, text_width(creature.name)) : 0;
            if (show_name)
                bar_y = std::clamp(bar_y, 1, viewport_height - font_height - 5);
            else
                bar_y = std::clamp(bar_y, 1, viewport_height - 5);

            auto color = std::array<std::uint8_t, 3>{};
            const auto health = std::min<std::uint8_t>(creature.health_percent, 100);
            const bool in_bounds = creature.local_x >= 0 &&
                creature.local_x < static_cast<std::int32_t>(map::map_viewport_tiles_x) &&
                creature.local_y >= 0 &&
                creature.local_y < static_cast<std::int32_t>(map::map_viewport_tiles_y);
            bool occluded = !in_bounds;
            if (in_bounds) {
                for (const auto& tile : snapshot.map->tiles) {
                    const auto position = map::viewport_map_position(*snapshot.map, tile);
                    if (!position || position->x != creature.local_x ||
                        position->y != creature.local_y ||
                        position->floor <= static_cast<std::int32_t>(floor) ||
                        position->floor > static_cast<std::int32_t>(floor_batches_.size()) - 1)
                        continue;
                    const auto objects = map::resolve_native_tile_object_order(tile, dat);
                    if (!objects || objects->empty()) continue;
                    const auto& top = *objects->front();
                    if (top.creature_id != 0 || top.type == 0 || top.type == 0x63)
                        continue;
                    const auto* item = dat.find_item(top.type);
                    if (item != nullptr && item->has_native_property(0)) {
                        occluded = true;
                        break;
                    }
                }
            }

            if (occluded) {
                color = {192, 192, 192};
            } else if (health < 4) {
                color = {96, 0, 0};
            } else if (health < 10) {
                color = {192, 0, 0};
            } else if (health < 30) {
                color = {192, 48, 48};
            } else if (health < 60) {
                color = {192, 192, 0};
            } else if (health < 95) {
                color = {96, 192, 96};
            } else {
                color = {0, 192, 0};
            }

            auto light = static_cast<std::int32_t>(
                light_at(creature.local_x, creature.local_y));
            if (creature.local_player) {
                const auto player_light = std::clamp<std::int32_t>(
                    creature.light_level, 2, 5) * 0x33;
                light = std::max(light, player_light);
            }
            const auto light_step = light / 5;
            for (auto& channel : color)
                channel = static_cast<std::uint8_t>(
                    static_cast<std::int32_t>(channel) * light_step / 0x33);

            auto name_x = (bar_x - name_width / 2) + 14;
            name_x = std::clamp(name_x, 1,
                std::max(1, viewport_width - name_width - 1));
            const auto name_y = bar_y - font_height;
            CreatureInfoOverlay overlay;
            overlay.name = creature.name;
            overlay.name_x = name_x;
            overlay.name_y = name_y;
            overlay.bar_x = bar_x;
            overlay.bar_y = bar_y;
            overlay.bar_fill_width = static_cast<std::int32_t>(health) / 4;
            overlay.color = color;

            const auto party_flag = creature.party_shield;
            const auto skull_flag = creature.skull;
            const bool local_player_in_party = local_party_shield == 1 ||
                local_party_shield == 3 || local_party_shield == 4;
            const auto valid_party = party_flag >= 1 && party_flag <= 4 &&
                (party_flag != 1 || local_player_in_party);
            const auto valid_skull = skull_flag >= 1 && skull_flag <= 4;
            if (valid_party)
                overlay.primary_mark_skin = static_cast<std::uint16_t>(0x100 + party_flag);
            else if (valid_skull)
                overlay.primary_mark_skin = static_cast<std::uint16_t>(0xf8 + skull_flag);
            if (valid_party && valid_skull)
                overlay.secondary_mark_skin = static_cast<std::uint16_t>(0xf8 + skull_flag);

            const auto primary_skin = overlay.primary_mark_skin;
            if (primary_skin != 0 && primary_skin < ui_skins.size()) {
                const auto& skin = ui_skins[primary_skin];
                auto mark_x = creature.sort_x - skin.width + 20 - camera_offset_x;
                auto mark_y = creature.sort_y + 1 - camera_offset_y;
                mark_x = std::clamp(mark_x, 1,
                    std::max(1, viewport_width - skin.width - 1));
                mark_y = std::clamp(mark_y, 1,
                    std::max(1, viewport_height - skin.height - 1));
                overlay.primary_mark_x = mark_x;
                overlay.primary_mark_y = mark_y;
                overlay.draw_primary_mark = valid_party || creature.party_relation == 0 ||
                    mark_blink_visible_;
                if (overlay.secondary_mark_skin != 0) {
                    overlay.secondary_mark_x = mark_x + 13;
                    overlay.secondary_mark_y = mark_y;
                    overlay.draw_secondary_mark = creature.party_relation == 0 ||
                        mark_blink_visible_;
                }
            }
            overlays.push_back(std::move(overlay));
        }
    }
    return overlays;
}

}
