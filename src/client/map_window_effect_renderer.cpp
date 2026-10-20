#include "client/map_window_effect_renderer.hpp"

#include "map/model.hpp"
#include "map/render_queue.hpp"
#include "map/tile_object_order.hpp"
#include "render/object_compositor.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <tuple>
#include <vector>

namespace tibia::client {
namespace {

constexpr std::uint32_t effect_texture_base = 0xe6000000U;
constexpr std::uint32_t projectile_texture_base = 0xe7000000U;
constexpr std::array<std::array<std::uint16_t, 2>, 8> missile_patterns{{
    {{2, 1}}, {{3, 1}}, {{3, 2}}, {{3, 3}},
    {{2, 3}}, {{1, 3}}, {{1, 2}}, {{1, 1}}
}};

resources::TibiaSprImage to_image(const render::SoftwareSurface& surface) {
    resources::TibiaSprImage image;
    std::memcpy(image.rgba.data(), surface.pixels().data(), image.rgba.size());
    return image;
}

bool projectile_is_occluded(const GameWorldSnapshot& snapshot,
    const GameWorldMapEffectSnapshot& effect,
    const resources::TibiaDatFile& dat,
    std::int32_t highest_active_floor) {
    if (!snapshot.map) return true;
    const auto& map = *snapshot.map;
    const auto in_viewport = effect.local_x >= 0 && effect.local_x < map::map_viewport_tiles_x &&
        effect.local_y >= 0 && effect.local_y < map::map_viewport_tiles_y;
    if (!in_viewport) {
        const auto visible_floor = map.center_floor < 8
            ? 7 - static_cast<std::int32_t>(map.center_floor) : 2;
        return effect.local_floor != visible_floor;
    }
    for (const auto& tile : map.tiles) {
        const auto position = map::viewport_map_position(map, tile);
        if (!position || position->x != effect.local_x || position->y != effect.local_y ||
            position->floor <= effect.local_floor || position->floor > highest_active_floor)
            continue;
        const auto objects = map::resolve_native_tile_object_order(tile, dat);
        if (!objects || objects->empty()) continue;
        const auto& object = *objects->front();
        if (object.creature_id != 0 || object.type == 0 || object.type == 0x63) continue;
        const auto* item = dat.find_item(object.type);
        if (item != nullptr && item->has_native_property(0)) return true;
    }
    return false;
}

}

std::size_t MapWindowEffectRenderer::draw_tile(
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
    std::int32_t origin_x,
    std::int32_t origin_y) {
    std::size_t drawn{};
    std::vector<const GameWorldMapEffectSnapshot*> position_effects;
    for (const auto& effect : snapshot.map_effects)
        if (effect.opcode == 0x83 && effect.local_floor == floor &&
            effect.local_x == local_x && effect.local_y == local_y && effect.effect != 0)
            position_effects.push_back(&effect);
    std::stable_sort(position_effects.begin(), position_effects.end(),
        [](const auto* left, const auto* right) {
            return left->insertion_order > right->insertion_order;
        });

    for (const auto* effect : position_effects) {
        const auto* object = dat.effects.find(effect->effect);
        if (object == nullptr || object->width == 0 || object->height == 0 ||
            object->layers == 0 || object->animation_frames == 0 ||
            effect->update_ticks >= object->animation_frames)
            continue;
        const auto phase = static_cast<std::uint16_t>(effect->update_ticks + 1U);
        for (std::uint16_t cell_x = 1; cell_x <= object->width; ++cell_x) {
            for (std::uint16_t cell_y = 1; cell_y <= object->height; ++cell_y) {
                for (std::uint16_t layer = 1; layer <= object->layers; ++layer) {
                    const auto sprite_id = object->sprite_id_at(
                        cell_x, cell_y, layer, 1, 1, 1, phase);
                    if (sprite_id == 0) continue;
                    const auto screen_x = static_cast<std::int64_t>(local_x) * 32 -
                        camera_offset_x + origin_x + 32 -
                        static_cast<std::int64_t>(cell_y) * 32;
                    const auto screen_y = static_cast<std::int64_t>(local_y) * 32 -
                        camera_offset_y + origin_y + 32 -
                        static_cast<std::int64_t>(cell_x) * 32;
                    if (screen_x < std::numeric_limits<int>::min() ||
                        screen_x > std::numeric_limits<int>::max() ||
                        screen_y < std::numeric_limits<int>::min() ||
                        screen_y > std::numeric_limits<int>::max())
                        continue;
                    const auto image = resources::decode_tibia_spr_image(
                        tibia_spr_bytes, tibia_spr, sprite_id);
                    renderer.upload(effect_texture_base + sprite_id, image);
                    renderer.draw_clipped(effect_texture_base + sprite_id,
                        static_cast<int>(screen_x), static_cast<int>(screen_y), 32, 32,
                        viewport_clip.x + origin_x, viewport_clip.y + origin_y,
                        viewport_clip.width, viewport_clip.height);
                    ++drawn;
                }
            }
        }
    }
    return drawn;
}

std::size_t MapWindowEffectRenderer::collect_floor_text_effects(
    const GameWorldSnapshot& snapshot,
    std::uint8_t floor,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    std::int32_t origin_x,
    std::int32_t origin_y,
    const TextDraw& draw_text) {
    std::size_t collected{};
    if (!draw_text) return collected;
    std::vector<const GameWorldMapEffectSnapshot*> text_effects;
    for (const auto& effect : snapshot.map_effects)
        if (effect.opcode == 0x84 && effect.local_floor == floor &&
            !effect.text.empty() && effect.text.size() < 10)
            text_effects.push_back(&effect);
    std::stable_sort(text_effects.begin(), text_effects.end(),
        [](const auto* left, const auto* right) {
            const auto left_cell = std::tie(left->local_x, left->local_y);
            const auto right_cell = std::tie(right->local_x, right->local_y);
            if (left_cell != right_cell) return left_cell < right_cell;
            return left->insertion_order > right->insertion_order;
        });

    for (const auto* effect : text_effects) {
        const auto x = static_cast<std::int64_t>(effect->local_x) * 32 -
            camera_offset_x + origin_x + 16;
        const auto y = static_cast<std::int64_t>(effect->local_y) * 32 -
            camera_offset_y + origin_y -
            static_cast<std::int64_t>(effect->update_ticks) * 2;
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
            y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
            continue;
        draw_text(effect->text, static_cast<int>(x), static_cast<int>(y), effect->text_color);
        ++collected;
    }
    return collected;
}

std::size_t MapWindowEffectRenderer::draw_deferred_projectiles(
    render::GraphicsRenderer& renderer,
    const GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    const MapWindowLayout& viewport_clip,
    std::int32_t highest_active_floor,
    std::int32_t camera_offset_x,
    std::int32_t camera_offset_y,
    std::int32_t origin_x,
    std::int32_t origin_y) {
    std::vector<const GameWorldMapEffectSnapshot*> projectiles;
    for (const auto& effect : snapshot.map_effects)
        if (effect.opcode == 0x85 && effect.renderable &&
            effect.projectile_direction < missile_patterns.size())
            projectiles.push_back(&effect);
    std::stable_sort(projectiles.begin(), projectiles.end(), [](const auto* left, const auto* right) {
        const auto left_cell = std::tie(left->local_floor, left->local_x, left->local_y);
        const auto right_cell = std::tie(right->local_floor, right->local_x, right->local_y);
        if (left_cell != right_cell) return left_cell < right_cell;
        return left->insertion_order > right->insertion_order;
    });

    std::size_t drawn{};
    for (const auto* effect : projectiles) {
        if (projectile_is_occluded(snapshot, *effect, dat, highest_active_floor)) continue;
        const auto* missile = dat.missiles.find(effect->effect);
        if (missile == nullptr || missile->width != 1 || missile->height != 1 ||
            missile->layers == 0 || missile->pattern_x != 3 ||
            missile->pattern_y != 3 || missile->pattern_z != 1)
            continue;
        const auto pattern = missile_patterns[effect->projectile_direction];
        const auto surface = render::compose_tibia_dat_object(tibia_spr_bytes,
            tibia_spr, *missile, pattern[0], pattern[1], 1, 1);
        const auto image = to_image(surface);
        const auto x = static_cast<std::int64_t>(effect->projectile_pixel_x) -
            camera_offset_x + origin_x + 32 -
            static_cast<std::int64_t>(pattern[0]) * 32 + effect->projectile_offset_x;
        const auto y = static_cast<std::int64_t>(effect->projectile_pixel_y) -
            camera_offset_y + origin_y + 32 -
            static_cast<std::int64_t>(pattern[1]) * 32 + effect->projectile_offset_y;
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max() ||
            y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max())
            continue;
        const auto texture_id = projectile_texture_base + static_cast<std::uint32_t>(effect->index);
        renderer.upload(texture_id, image);
        renderer.draw_clipped(texture_id, static_cast<int>(x), static_cast<int>(y), 32, 32,
            viewport_clip.x + origin_x, viewport_clip.y + origin_y,
            viewport_clip.width, viewport_clip.height);
        ++drawn;
    }
    return drawn;
}

}
