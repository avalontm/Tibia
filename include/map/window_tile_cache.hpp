#pragma once

#include "map/render_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tibia::map {

class MapWindowTileCache {
public:
    MapWindowTileCache();

    void begin_frame() noexcept;

    void invalidate() noexcept;

    [[nodiscard]] TileRenderCellProvider make_provider(
        const DecodedMap& map);

    [[nodiscard]] TileRenderCell& cell_at(
        std::int32_t column,
        std::int32_t row);

private:
    std::array<TileRenderCell, map_cache_columns * map_cache_rows> cells_;
};

}
