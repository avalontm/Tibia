#include "client/map_window_tile_surface_renderer.hpp"

#include "render/software_surface.hpp"
#include "resources/tibia_spr.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tibia::client {
namespace {

constexpr std::uint32_t map_tile_texture_base = 0xe2000000U;
constexpr std::uint32_t map_picture_texture_base = 0xe3000000U;

std::size_t cell_index(std::int32_t local_x, std::int32_t local_y,
    const MapWindowItemFrame& frame) {
    return map::map_cache_coordinate(local_x, local_y,
        frame.ring_offset_x, frame.ring_offset_y).index;
}

resources::TibiaSprImage to_image(const render::SoftwareSurface& surface) {
    resources::TibiaSprImage image;
    std::memcpy(image.rgba.data(), surface.pixels().data(), image.rgba.size());
    return image;
}

}

MapWindowTileSurfaceRenderer::MapWindowTileSurfaceRenderer() {
    cells_.reserve(map::map_cache_columns * map::map_cache_rows);
    for (std::int32_t i = 0; i < map::map_cache_columns * map::map_cache_rows; ++i)
        cells_.emplace_back(map::map_tile_pixels, map::map_tile_pixels,
            render::RgbaColor{0, 0, 0, 0});
}

void MapWindowTileSurfaceRenderer::invalidate() noexcept {
    direct_sprites_.invalidate();
    for (auto& cell : cells_)
        cell.clear({0, 0, 0, 0});
    cell_texture_uploaded_.fill(false);
    cell_texture_dirty_.fill(true);
    uploaded_picture_ids_.clear();
}

MapWindowTileSurfaceDrawStats MapWindowTileSurfaceRenderer::draw(
    render::GraphicsRenderer& renderer,
    const MapWindowItemFrame& frame,
    std::span<const std::uint8_t> tibia_spr_bytes,
    const resources::TibiaSprFile& tibia_spr,
    std::span<const std::uint8_t> tibia_pic_bytes,
    const resources::TibiaPicFile& tibia_pic,
    const MapWindowLayout& viewport_clip,
    std::int32_t origin_x,
    std::int32_t origin_y,
    const std::function<void(std::int32_t)>& after_floor,
    const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& during_tile_pass,
    const std::function<void()>& before_overlays,
    const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& after_tile) {
    std::array<bool, map::map_cache_columns * map::map_cache_rows> submitted{};
    struct TileOverlay {
        std::int32_t floor;
        std::int32_t local_x;
        std::int32_t local_y;
    };
    std::vector<TileOverlay> tile_overlays;
    std::unordered_map<std::uint16_t, resources::TibiaSprImage> sprite_images;
    std::unordered_map<std::uint16_t, resources::TibiaSprImage> picture_images;
    std::size_t draw_count{};

    const auto get_sprite = [&](std::uint16_t sprite_id) -> const resources::TibiaSprImage& {
        const auto found = sprite_images.find(sprite_id);
        if (found != sprite_images.end()) return found->second;
        auto image = resources::decode_tibia_spr_image(
            tibia_spr_bytes, tibia_spr, sprite_id);
        return sprite_images.emplace(sprite_id, std::move(image)).first->second;
    };
    const auto get_picture = [&](std::uint16_t picture_id) -> const resources::TibiaSprImage& {
        const auto found = picture_images.find(picture_id);
        if (found != picture_images.end()) return found->second;
        auto image = resources::decode_tibia_pic_tile(
            tibia_pic_bytes, tibia_pic, picture_id, 0, 0);
        return picture_images.emplace(picture_id, std::move(image)).first->second;
    };
    const auto draw_picture_request = [&](const map::TileDrawRequest& request) {
        if (request.selector <= 0 || request.selector >
            static_cast<std::int32_t>(tibia_pic.pictures.size()))
            throw std::out_of_range("map draw references an invalid Tibia.pic picture");
        const auto picture_id = static_cast<std::uint16_t>(request.selector);
        const auto texture_id = map_picture_texture_base + picture_id;
        if (!uploaded_picture_ids_.contains(picture_id)) {
            renderer.upload(texture_id, get_picture(picture_id));
            uploaded_picture_ids_.insert(picture_id);
        }
        if (request.path == map::TileDrawPath::direct_sprite) {
            renderer.draw_clipped(texture_id, request.destination_x + origin_x,
                request.destination_y + origin_y, map::map_tile_pixels,
                map::map_tile_pixels, viewport_clip.x, viewport_clip.y,
                viewport_clip.width, viewport_clip.height);
        } else {
            auto clip_x = request.clip_x + origin_x;
            auto clip_y = request.clip_y + origin_y;
            auto clip_width = request.clip_width;
            auto clip_height = request.clip_height;
            const auto right = std::min(clip_x + clip_width,
                viewport_clip.x + viewport_clip.width);
            const auto bottom = std::min(clip_y + clip_height,
                viewport_clip.y + viewport_clip.height);
            clip_x = std::max(clip_x, viewport_clip.x);
            clip_y = std::max(clip_y, viewport_clip.y);
            clip_width = right - clip_x;
            clip_height = bottom - clip_y;
            if (clip_width <= 0 || clip_height <= 0) return;
            renderer.draw_clipped(texture_id,
                request.destination_x + origin_x,
                request.destination_y + origin_y,
                map::map_tile_pixels, map::map_tile_pixels,
                clip_x, clip_y, clip_width, clip_height);
        }
    };
    const auto flush_cell = [&](std::int32_t local_x, std::int32_t local_y,
        std::int32_t screen_x, std::int32_t screen_y) {
        const auto index = cell_index(local_x, local_y, frame);
        const auto texture_id = map_tile_texture_base + static_cast<std::uint32_t>(index);
        if (!cell_texture_uploaded_[index] || cell_texture_dirty_[index]) {
            renderer.upload(texture_id, to_image(cells_[index]));
            cell_texture_uploaded_[index] = true;
            cell_texture_dirty_[index] = false;
        }
        renderer.draw_clipped(texture_id, origin_x + screen_x, origin_y + screen_y,
            map::map_tile_pixels, map::map_tile_pixels,
            viewport_clip.x, viewport_clip.y,
            viewport_clip.width, viewport_clip.height);
        submitted[index] = true;
        ++draw_count;
    };
    const auto render_cached_request = [&](std::int32_t local_x, std::int32_t local_y,
        const map::TileDrawRequest& request) {
        const auto index = cell_index(local_x, local_y, frame);
        const auto offset_x = request.destination_x - request.clip_x;
        const auto offset_y = request.destination_y - request.clip_y;
        if (request.asset == map::TileDrawAsset::tibia_pic) {
            if (request.selector <= 0 || request.selector >
                static_cast<std::int32_t>(tibia_pic.pictures.size()))
                throw std::out_of_range("map cache references an invalid Tibia.pic picture");
            cells_[index].draw(get_picture(static_cast<std::uint16_t>(request.selector)),
                offset_x, offset_y);
        } else {
            if (request.selector <= 0 || request.selector >
                static_cast<std::int32_t>(tibia_spr.sprite_offsets.size()))
                throw std::out_of_range("map cache references an invalid Tibia.spr sprite");
            cells_[index].draw(get_sprite(static_cast<std::uint16_t>(request.selector)),
                offset_x, offset_y);
        }
        cell_texture_dirty_[index] = true;
    };

    for (const auto& tile : frame.visible_tiles) {
        if (tile.batch.status != map::TileRenderStatus::queued) continue;
        const auto index = cell_index(tile.local_x, tile.local_y, frame);
        if (tile.batch.reset_cache) {
            cells_[index].clear({0, 0, 0, 0});
            cell_texture_dirty_[index] = true;
        }
        for (const auto& request : tile.batch.draws)
            render_cached_request(tile.local_x, tile.local_y, request);
    }

    const auto finish_floor = [&](std::int32_t floor) {
        if (after_floor) after_floor(floor);
    };

    const auto draw_visit = [&](const MapWindowItemVisitFrame& visit) {
        const std::array<const std::vector<render::MapItemObjectSpriteRoute>*, 2> routes{
            &visit.standard_route.routes, &visit.deferred_route.routes};
        for (const auto* route_list : routes) {
            for (const auto& object_route : *route_list) {
                for (const auto& route_batch : object_route.route.batches) {
                    for (const auto& tile_batch : route_batch.tile_batches) {
                        const auto index = cell_index(tile_batch.local_x, tile_batch.local_y, frame);
                        if (tile_batch.batch.reset_cache) {
                            cells_[index].clear({0, 0, 0, 0});
                            cell_texture_dirty_[index] = true;
                        }
                        for (const auto& request : tile_batch.batch.draws) {
                            if (request.target == map::TileDrawTarget::tile_cache) {
                                render_cached_request(tile_batch.local_x,
                                    tile_batch.local_y, request);
                            } else if (!submitted[index]) {
                                flush_cell(tile_batch.local_x, tile_batch.local_y,
                                    tile_batch.local_x * map::map_tile_pixels - frame.camera_offset_x,
                                    tile_batch.local_y * map::map_tile_pixels - frame.camera_offset_y);
                            }
                            if (request.target == map::TileDrawTarget::viewport) {
                                if (request.asset == map::TileDrawAsset::tibia_pic)
                                    draw_picture_request(request);
                                else
                                    direct_sprites_.draw_request(renderer, request,
                                        tibia_spr_bytes, tibia_spr, origin_x, origin_y,
                                        &viewport_clip);
                                ++draw_count;
                            }
                        }
                        if (route_batch.status == map::ObjectSpriteRouteStatus::viewport_draw_queued &&
                            tile_batch.batch.status == map::TileRenderStatus::queued && !submitted[index])
                            flush_cell(tile_batch.local_x, tile_batch.local_y,
                                tile_batch.batch.update.screen_x,
                                tile_batch.batch.update.screen_y);
                    }
                    if (route_batch.viewport_draw) {
                        const auto& request = *route_batch.viewport_draw;
                        if (request.asset == map::TileDrawAsset::tibia_pic)
                            draw_picture_request(request);
                        else
                            direct_sprites_.draw_request(renderer, request,
                                tibia_spr_bytes, tibia_spr, origin_x, origin_y,
                                &viewport_clip);
                        ++draw_count;
                    }
                }
            }
        }
    };

    auto current_floor = -1;
    std::size_t visit_index{};
    while (visit_index < frame.visits.size()) {
        const auto& primary_visit = frame.visits[visit_index];
        const auto floor = static_cast<std::int32_t>(primary_visit.visit.floor);
        if (current_floor != floor) {
            if (current_floor >= 0) finish_floor(current_floor);
            current_floor = floor;
        }
        auto group_end = visit_index + 1;
        while (group_end < frame.visits.size() &&
            frame.visits[group_end].visit.floor == primary_visit.visit.floor &&
            frame.visits[group_end].visit.neighboring_object_pass)
            ++group_end;
        for (auto index = visit_index; index < group_end; ++index)
            draw_visit(frame.visits[index]);

        if (!primary_visit.visit.neighboring_object_pass) {
            if (during_tile_pass) {
                const auto tile = std::find_if(frame.visible_tiles.begin(),
                    frame.visible_tiles.end(), [&primary_visit](const auto& candidate) {
                        return candidate.local_x == primary_visit.visit.local_x &&
                            candidate.local_y == primary_visit.visit.local_y;
                    });
                const auto cell = cell_index(primary_visit.visit.local_x,
                    primary_visit.visit.local_y, frame);
                if (tile != frame.visible_tiles.end() &&
                    tile->batch.status == map::TileRenderStatus::queued &&
                    !submitted[cell]) {
                    flush_cell(primary_visit.visit.local_x,
                        primary_visit.visit.local_y, tile->batch.update.screen_x,
                        tile->batch.update.screen_y);
                }
                during_tile_pass(floor, primary_visit.visit.local_x,
                    primary_visit.visit.local_y);
            }
            if (after_tile)
                tile_overlays.push_back({floor, primary_visit.visit.local_x,
                    primary_visit.visit.local_y});
        }
        visit_index = group_end;
    }
    if (current_floor >= 0) finish_floor(current_floor);
    else {
        for (std::int32_t floor = 0; floor <= frame.highest_active_floor; ++floor) {
            for (const auto& tile : frame.visible_tiles) {
                if (tile.batch.status != map::TileRenderStatus::queued) continue;
                const auto cell = cell_index(tile.local_x, tile.local_y, frame);
                if (during_tile_pass) {
                    if (!submitted[cell])
                        flush_cell(tile.local_x, tile.local_y,
                            tile.batch.update.screen_x, tile.batch.update.screen_y);
                    during_tile_pass(floor, tile.local_x, tile.local_y);
                }
                if (after_tile)
                    tile_overlays.push_back({floor, tile.local_x, tile.local_y});
            }
            if (after_floor) after_floor(floor);
        }
    }
    for (const auto& tile : frame.visible_tiles) {
        if (tile.batch.status != map::TileRenderStatus::queued) continue;
        const auto index = cell_index(tile.local_x, tile.local_y, frame);
        if (!submitted[index] || cell_texture_dirty_[index])
            flush_cell(tile.local_x, tile.local_y,
                tile.batch.update.screen_x, tile.batch.update.screen_y);
    }
    if (before_overlays) before_overlays();
    if (after_tile) {
        for (const auto& overlay : tile_overlays)
            after_tile(overlay.floor, overlay.local_x, overlay.local_y);
    }
    MapWindowTileSurfaceDrawStats stats;
    stats.draw_calls = draw_count;
    std::array<bool, map::map_cache_columns * map::map_cache_rows> inspected{};
    for (const auto& tile : frame.visible_tiles) {
        if (tile.batch.status != map::TileRenderStatus::queued) continue;
        const auto index = cell_index(tile.local_x, tile.local_y, frame);
        if (inspected[index]) continue;
        inspected[index] = true;
        bool has_pixels = false;
        const auto& pixels = cells_[index].pixels();
        for (std::size_t pixel = 3; pixel < pixels.size(); pixel += 4) {
            if (pixels[pixel] == 0) continue;
            ++stats.nontransparent_pixels;
            has_pixels = true;
        }
        if (has_pixels) ++stats.visible_cells_with_pixels;
    }
    return stats;
}

}
