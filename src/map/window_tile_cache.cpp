#include "map/window_tile_cache.hpp"

#include <stdexcept>

namespace tibia::map {

MapWindowTileCache::MapWindowTileCache() {
    for (std::int32_t column = 0; column < map_cache_columns; ++column) {
        for (std::int32_t row = 0; row < map_cache_rows; ++row) {
            auto& cell = cells_[static_cast<std::size_t>(column * map_cache_rows + row)];
            cell.origin_x = column * map_tile_pixels;
            cell.origin_y = row * map_tile_pixels;
            cell.objects.reserve(10);
        }
    }
}

void MapWindowTileCache::begin_frame() noexcept {
    for (auto& cell : cells_) {
        cell.current_count = 0;
        cell.submitted = false;
    }
}

void MapWindowTileCache::invalidate() noexcept {
    for (auto& cell : cells_) {
        cell.current_count = 0;
        cell.cached_count = 0;
        cell.submitted = false;
    }
}

TileRenderCellProvider MapWindowTileCache::make_provider(const DecodedMap& map) {
    const auto ring_offset_x = map.center_x % map_cache_columns;
    const auto ring_offset_y = map.center_y % map_cache_rows;
    return [this, ring_offset_x, ring_offset_y](
        std::int32_t local_x, std::int32_t local_y) -> TileRenderCell& {
        const auto coordinate = map_cache_coordinate(
            local_x, local_y, ring_offset_x, ring_offset_y);
        return cells_[coordinate.index];
    };
}

TileRenderCell& MapWindowTileCache::cell_at(
    std::int32_t column,
    std::int32_t row) {
    if (column < 0 || column >= map_cache_columns ||
        row < 0 || row >= map_cache_rows)
        throw std::out_of_range("map cache coordinate is outside its dimensions");
    return cells_[static_cast<std::size_t>(column * map_cache_rows + row)];
}

}
