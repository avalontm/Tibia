#include "client/map_window_direct_sprite_renderer.hpp"

#include "resources/tibia_spr.hpp"

#include <stdexcept>

namespace tibia::client {
namespace {

constexpr std::uint32_t map_sprite_texture_base = 0xe1000000U;

std::uint32_t texture_id(std::uint16_t sprite_id) noexcept {
    return map_sprite_texture_base + sprite_id;
}

}

void MapWindowDirectSpriteRenderer::invalidate() noexcept {
    uploaded_sprite_ids_.clear();
}

std::size_t MapWindowDirectSpriteRenderer::draw(
    render::GraphicsRenderer& renderer,
    const MapWindowItemFrame& frame,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr) {
    std::size_t draw_count{};
    for (const auto& visit : frame.visits) {
        for (const auto& object_route : visit.standard_route.routes) {
            for (const auto& batch : object_route.route.batches) {
                for (const auto& tile_batch : batch.tile_batches) {
                    for (const auto& request : tile_batch.batch.draws) {
                        if (request.target != map::TileDrawTarget::viewport ||
                            request.asset != map::TileDrawAsset::tibia_spr) continue;
                        draw_request(renderer, request, tibia_spr_bytes, tibia_spr);
                        ++draw_count;
                    }
                }
                if (batch.viewport_draw &&
                    batch.viewport_draw->asset == map::TileDrawAsset::tibia_spr) {
                    draw_request(renderer, *batch.viewport_draw,
                        tibia_spr_bytes, tibia_spr);
                    ++draw_count;
                }
            }
        }
    }
    return draw_count;
}

void MapWindowDirectSpriteRenderer::draw_request(
    render::GraphicsRenderer& renderer,
    const map::TileDrawRequest& request,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::int32_t origin_x,
    std::int32_t origin_y,
    const MapWindowLayout* viewport_clip) {
    if (request.target != map::TileDrawTarget::viewport || request.selector <= 0 ||
        request.selector > static_cast<std::int32_t>(tibia_spr.sprite_offsets.size()))
        throw std::out_of_range("map viewport draw references an invalid Tibia.spr sprite");

    const auto sprite_id = static_cast<std::uint16_t>(request.selector);
    const auto id = texture_id(sprite_id);
    if (!uploaded_sprite_ids_.contains(sprite_id)) {
        renderer.upload(id, resources::decode_tibia_spr_image(
            tibia_spr_bytes, tibia_spr, sprite_id));
        uploaded_sprite_ids_.insert(sprite_id);
    }

    if (request.path == map::TileDrawPath::direct_sprite) {
        if (viewport_clip) {
            renderer.draw_clipped(id, request.destination_x + origin_x,
                request.destination_y + origin_y, map::map_tile_pixels,
                map::map_tile_pixels, viewport_clip->x, viewport_clip->y,
                viewport_clip->width, viewport_clip->height);
        } else {
            renderer.draw(id, request.destination_x + origin_x,
                request.destination_y + origin_y, map::map_tile_pixels,
                map::map_tile_pixels);
        }
        return;
    }

    auto clip_x = request.clip_x + origin_x;
    auto clip_y = request.clip_y + origin_y;
    auto clip_width = request.clip_width;
    auto clip_height = request.clip_height;
    if (viewport_clip) {
        const auto right = std::min(clip_x + clip_width,
            viewport_clip->x + viewport_clip->width);
        const auto bottom = std::min(clip_y + clip_height,
            viewport_clip->y + viewport_clip->height);
        clip_x = std::max(clip_x, viewport_clip->x);
        clip_y = std::max(clip_y, viewport_clip->y);
        clip_width = right - clip_x;
        clip_height = bottom - clip_y;
        if (clip_width <= 0 || clip_height <= 0) return;
    }

    renderer.draw_clipped(id, request.destination_x + origin_x,
        request.destination_y + origin_y,
        map::map_tile_pixels, map::map_tile_pixels,
        clip_x, clip_y, clip_width, clip_height);
}

}
