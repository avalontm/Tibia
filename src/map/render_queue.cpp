#include "map/render_queue.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace tibia::map {
namespace {

bool is_in_view(std::int32_t local_x, std::int32_t local_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y) noexcept {
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y) return false;
    const auto screen_x = local_x * map_tile_pixels - camera_offset_x;
    const auto screen_y = local_y * map_tile_pixels - camera_offset_y;
    return screen_x > -map_tile_pixels && screen_y > -map_tile_pixels &&
        screen_x < map_viewport_width && screen_y < map_viewport_height;
}

std::int32_t tile_index_at(std::int32_t coordinate) noexcept {
    const auto adjusted = static_cast<std::int64_t>(coordinate) + map_tile_pixels;
    const auto quotient = adjusted / map_tile_pixels;
    const auto remainder = adjusted % map_tile_pixels;
    return static_cast<std::int32_t>(quotient - (remainder < 0 ? 1 : 0) - 1);
}

TileDrawRequest make_cache_draw(const TileRenderCell& cell,
    const TileRenderObject& object) {
    const auto direct = object.draw_selector == 0;
    return {
        direct ? TileDrawPath::direct_sprite : TileDrawPath::textured,
        TileDrawTarget::tile_cache,
        direct ? TileDrawAsset::tibia_pic : TileDrawAsset::tibia_spr,
        cell.origin_x + object.offset_x,
        cell.origin_y + object.offset_y,
        direct ? 7 : object.draw_selector,
        cell.origin_x,
        cell.origin_y,
        map_tile_pixels,
        map_tile_pixels
    };
}

TileDrawRequest make_viewport_draw(const TileRenderCell& cell,
    const TileRenderObject& object, std::int32_t screen_x,
    std::int32_t screen_y) {
    auto request = make_cache_draw(cell, object);
    request.target = TileDrawTarget::viewport;
    request.destination_x = screen_x + object.offset_x;
    request.destination_y = screen_y + object.offset_y;
    request.clip_x = screen_x;
    request.clip_y = screen_y;
    return request;
}

TileDrawRequest clip_textured_viewport_draw(TileDrawRequest request) {
    const auto left = std::max(request.clip_x, 0);
    const auto top = std::max(request.clip_y, 0);
    const auto right = std::min(request.clip_x + request.clip_width,
        map_viewport_width);
    const auto bottom = std::min(request.clip_y + request.clip_height,
        map_viewport_height);
    request.clip_x = left;
    request.clip_y = top;
    request.clip_width = std::max(0, right - left);
    request.clip_height = std::max(0, bottom - top);
    return request;
}

}

MapCacheCoordinate map_cache_coordinate(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t ring_offset_x, std::int32_t ring_offset_y) {
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y)
        throw std::out_of_range("map viewport coordinate is outside its dimensions");
    if (ring_offset_x < 0 || ring_offset_x >= map_cache_columns ||
        ring_offset_y < 0 || ring_offset_y >= map_cache_rows)
        throw std::out_of_range("map cache ring offset is outside its dimensions");
    const auto column = (ring_offset_x + local_x) % map_cache_columns;
    const auto row = (ring_offset_y + local_y) % map_cache_rows;
    return {column, row, static_cast<std::size_t>(column * map_cache_rows + row)};
}

MapCacheCoordinate map_cache_coordinate(
    std::int32_t local_x, std::int32_t local_y,
    const DecodedMap& map) {
    return map_cache_coordinate(local_x, local_y,
        map.center_x % map_cache_columns,
        map.center_y % map_cache_rows);
}

TileRenderCellLookup make_filtered_tile_render_cell_lookup(
    const MapRenderFilterGrid& filter_grid,
    const TileRenderCellProvider& cell_provider) {
    if (!cell_provider)
        throw std::invalid_argument("filtered map cells require a render-cell provider");
    return [filter_grid, cell_provider](std::int32_t local_x, std::int32_t local_y) {
        auto& cell = cell_provider(local_x, local_y);
        return TileRenderCellCandidate{&cell, filter_grid.value_at(local_x, local_y)};
    };
}

TileRenderBatch queue_visible_tile(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    TileRenderCell& cell) {
    TileRenderBatch batch;
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y) return batch;
    const auto screen_x = local_x * map_tile_pixels - camera_offset_x;
    const auto screen_y = local_y * map_tile_pixels - camera_offset_y;
    if (!is_in_view(local_x, local_y, camera_offset_x, camera_offset_y)) return batch;
    if (cell.submitted) {
        batch.status = TileRenderStatus::already_submitted;
        return batch;
    }
    if (cell.current_count < cell.cached_count) {
        batch.reset_cache = true;
        if (cell.current_count > cell.objects.size()) {
            batch.status = TileRenderStatus::unresolved_cell_data;
            return batch;
        }
        batch.draws.reserve(cell.current_count);
        for (std::size_t i = 0; i < cell.current_count; ++i) {
            const auto& object = cell.objects[i];
            batch.draws.push_back(make_cache_draw(cell, object));
        }
        cell.cached_count = cell.current_count;
    }
    cell.submitted = true;
    batch.status = TileRenderStatus::queued;
    batch.update = {screen_x, screen_y, cell.origin_x, cell.origin_y};
    return batch;
}

TileObjectUpdateBatch add_tile_render_object(
    std::int32_t local_x, std::int32_t local_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    TileRenderCell& cell, std::int32_t selector,
    std::int32_t offset_x, std::int32_t offset_y) {
    TileObjectUpdateBatch batch;
    if (!is_in_view(local_x, local_y, camera_offset_x, camera_offset_y))
        return batch;

    const auto screen_x = local_x * map_tile_pixels - camera_offset_x;
    const auto screen_y = local_y * map_tile_pixels - camera_offset_y;
    const TileRenderObject object{selector, offset_x, offset_y};

    if (cell.current_count == 10 && !cell.submitted) {
        auto submitted = queue_visible_tile(local_x, local_y,
            camera_offset_x, camera_offset_y, cell);
        if (submitted.status == TileRenderStatus::unresolved_cell_data) {
            batch.status = TileObjectUpdateStatus::unresolved_cell_data;
            return batch;
        }
        batch.draws = std::move(submitted.draws);
    }

    if (cell.submitted) {
        auto request = make_viewport_draw(cell, object, screen_x, screen_y);
        if (request.path == TileDrawPath::textured)
            request = clip_textured_viewport_draw(request);
        batch.draws.push_back(request);
        batch.status = TileObjectUpdateStatus::added_to_submitted_cell;
        return batch;
    }

    if (cell.current_count > cell.cached_count ||
        cell.current_count > cell.objects.size() ||
        cell.cached_count > cell.objects.size()) {
        batch.status = TileObjectUpdateStatus::unresolved_cell_data;
        return batch;
    }

    if (cell.current_count < cell.cached_count &&
        cell.objects[cell.current_count] == object) {
        ++cell.current_count;
        batch.status = TileObjectUpdateStatus::added_to_cache;
        return batch;
    }

    if (cell.current_count == cell.cached_count) {
        batch.draws.push_back(make_cache_draw(cell, object));
        if (cell.current_count == cell.objects.size())
            cell.objects.push_back(object);
        else
            cell.objects[cell.current_count] = object;
        ++cell.cached_count;
        ++cell.current_count;
        batch.status = TileObjectUpdateStatus::added_to_cache;
        return batch;
    }

    const auto existing_count = cell.current_count;
    batch.reset_cache = true;
    for (std::size_t i = 0; i < existing_count; ++i)
        batch.draws.push_back(make_cache_draw(cell, cell.objects[i]));
    batch.draws.push_back(make_cache_draw(cell, object));
    if (cell.current_count == cell.objects.size())
        cell.objects.push_back(object);
    else
        cell.objects[cell.current_count] = object;
    ++cell.current_count;
    cell.cached_count = cell.current_count;
    batch.status = TileObjectUpdateStatus::added_to_cache;
    return batch;
}

ObjectSpriteRouteBatch route_object_sprite(
    std::int32_t screen_x, std::int32_t screen_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t sprite_id, bool bypass_tile_cache,
    std::int32_t maximum_cell_value,
    const TileRenderCellLookup& cell_lookup) {
    ObjectSpriteRouteBatch batch;
    if (!cell_lookup)
        throw std::invalid_argument("object sprites require a map-cell lookup");

    const auto map_x = screen_x + camera_offset_x;
    const auto map_y = screen_y + camera_offset_y;
    const auto left_x = tile_index_at(map_x);
    const auto top_y = tile_index_at(map_y);
    const auto right_x = left_x + 1;
    const auto bottom_y = top_y + 1;
    const auto offset_x = map_x - left_x * map_tile_pixels;
    const auto offset_y = map_y - top_y * map_tile_pixels;
    const std::array<MapTileRenderBatch, 4> candidates{{
        {left_x, top_y, {}},
        {right_x, top_y, {}},
        {left_x, bottom_y, {}},
        {right_x, bottom_y, {}}
    }};
    const std::array<std::int32_t, 4> offsets_x{{
        offset_x, offset_x - map_tile_pixels,
        offset_x, offset_x - map_tile_pixels
    }};
    const std::array<std::int32_t, 4> offsets_y{{
        offset_y, offset_y,
        offset_y - map_tile_pixels, offset_y - map_tile_pixels
    }};
    const std::array<bool, 4> needed{{
        true, offset_x != 0, offset_y != 0, offset_x != 0 && offset_y != 0
    }};

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (!needed[i]) continue;
        const auto local_x = candidates[i].local_x;
        const auto local_y = candidates[i].local_y;
        if (local_x < 0 || local_x >= map_viewport_tiles_x ||
            local_y < 0 || local_y >= map_viewport_tiles_y)
            continue;
        const auto candidate = cell_lookup(local_x, local_y);
        if (candidate.cell == nullptr || !candidate.filter_value) {
            batch.status = ObjectSpriteRouteStatus::unresolved_cell_data;
            return batch;
        }
        if (*candidate.filter_value > maximum_cell_value)
            continue;

        if (bypass_tile_cache) {
            auto tile_batch = queue_visible_tile(local_x, local_y,
                camera_offset_x, camera_offset_y, *candidate.cell);
            if (tile_batch.status == TileRenderStatus::unresolved_cell_data) {
                batch.status = ObjectSpriteRouteStatus::unresolved_cell_data;
                return batch;
            }
            batch.tile_batches.push_back({local_x, local_y, std::move(tile_batch)});
        } else {
            auto object_batch = add_tile_render_object(local_x, local_y,
                camera_offset_x, camera_offset_y, *candidate.cell, sprite_id,
                offsets_x[i], offsets_y[i]);
            if (object_batch.status == TileObjectUpdateStatus::unresolved_cell_data) {
                batch.status = ObjectSpriteRouteStatus::unresolved_cell_data;
                return batch;
            }
            if (object_batch.status == TileObjectUpdateStatus::outside_view) continue;
            batch.tile_batches.push_back({local_x, local_y,
                {TileRenderStatus::queued, std::move(object_batch.draws), {},
                    object_batch.reset_cache}});
        }
    }

    if (batch.tile_batches.empty()) return batch;
    if (bypass_tile_cache) {
        batch.viewport_draw = TileDrawRequest{
            TileDrawPath::direct_sprite,
            TileDrawTarget::viewport,
            TileDrawAsset::tibia_spr,
            screen_x,
            screen_y,
            sprite_id,
            0,
            0,
            map_tile_pixels,
            map_tile_pixels
        };
        batch.status = ObjectSpriteRouteStatus::viewport_draw_queued;
    } else {
        batch.status = ObjectSpriteRouteStatus::cached_cells_updated;
    }
    return batch;
}

std::vector<MapTileRenderBatch> queue_visible_map_tiles(
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    const TileRenderCellProvider& cell_provider) {
    std::vector<MapTileRenderBatch> batches;
    batches.reserve(map_viewport_tiles_x * map_viewport_tiles_y);
    for (std::int32_t local_x = 0; local_x < map_viewport_tiles_x; ++local_x) {
        for (std::int32_t local_y = 0; local_y < map_viewport_tiles_y; ++local_y) {
            if (!is_in_view(local_x, local_y, camera_offset_x, camera_offset_y)) {
                batches.push_back({local_x, local_y, {}});
                continue;
            }
            if (!cell_provider)
                throw std::invalid_argument("visible map cells require a render-cell provider");
            batches.push_back({local_x, local_y,
                queue_visible_tile(local_x, local_y, camera_offset_x, camera_offset_y,
                    cell_provider(local_x, local_y))});
        }
    }
    return batches;
}

MapEffectProjection project_map_effect(
    const LightField& field, std::int32_t screen_x, std::int32_t screen_y,
    std::int32_t camera_offset_x, std::int32_t camera_offset_y,
    std::int32_t maximum_field_value, std::int32_t texture,
    bool partial_effect, std::int32_t extent, std::int32_t extra) {
    MapEffectProjection projection;
    projection.screen_x = screen_x;
    projection.screen_y = screen_y;
    projection.texture = texture;
    projection.extent = extent;
    projection.extra = extra;

    const auto world_x = camera_offset_x + screen_x;
    const auto world_y = camera_offset_y + screen_y;
    const auto right_tile = (world_x + map_tile_pixels) / map_tile_pixels;
    const auto bottom_tile = (world_y + map_tile_pixels) / map_tile_pixels;
    const auto left_tile = right_tile - 1;
    const auto top_tile = bottom_tile - 1;
    const auto offset_x = (world_x + map_tile_pixels) % map_tile_pixels;
    const auto offset_y = (world_y + map_tile_pixels) % map_tile_pixels;
    const std::array<MapEffectTarget, 4> candidates{{
        {left_tile, top_tile, offset_x, offset_y},
        {right_tile, top_tile, offset_x - map_tile_pixels, offset_y},
        {left_tile, bottom_tile, offset_x, offset_y - map_tile_pixels},
        {right_tile, bottom_tile, offset_x - map_tile_pixels, offset_y - map_tile_pixels}
    }};
    const std::array<bool, 4> needed{{
        true, offset_x != 0, offset_y != 0, offset_x != 0 && offset_y != 0
    }};
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (!needed[i]) continue;
        const auto match = field.field_at_most(
            candidates[i].local_x, candidates[i].local_y, maximum_field_value);
        if (!match.has_value()) {
            projection.targets.clear();
            projection.path = MapEffectPath::unresolved;
            return projection;
        }
        if (*match) projection.targets.push_back(candidates[i]);
    }
    if (partial_effect || extent != 0) {
        if (!projection.targets.empty())
            projection.path = extent == 0 ? MapEffectPath::direct : MapEffectPath::extended;
    } else if (!projection.targets.empty()) {
        projection.path = MapEffectPath::tile_overlays;
    }
    return projection;
}

}
