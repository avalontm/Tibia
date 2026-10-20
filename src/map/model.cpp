#include "map/model.hpp"

#include "map/render_queue.hpp"

namespace tibia::map {

std::optional<ViewportTilePosition> viewport_tile_position(
    const DecodedMap& map, const Tile& tile) noexcept {
    if ((map.center_x < 8 && tile.x == 0) ||
        (map.center_x > 65526 && tile.x == 65535) ||
        (map.center_y < 6 && tile.y == 0) ||
        (map.center_y > 65528 && tile.y == 65535))
        return std::nullopt;
    const auto local_x = static_cast<std::int32_t>(tile.x) - map.center_x + 8;
    const auto local_y = static_cast<std::int32_t>(tile.y) - map.center_y + 6;
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y)
        return std::nullopt;
    return ViewportTilePosition{
        static_cast<std::uint8_t>(local_x), static_cast<std::uint8_t>(local_y)};
}

std::optional<ViewportMapPosition> viewport_map_position(
    const DecodedMap& map, const Tile& tile) noexcept {
    if ((map.center_x < 8 && tile.x == 0) ||
        (map.center_x > 65526 && tile.x == 65535) ||
        (map.center_y < 6 && tile.y == 0) ||
        (map.center_y > 65528 && tile.y == 65535))
        return std::nullopt;
    const auto floor_delta = static_cast<std::int32_t>(map.center_floor) - tile.floor;
    const auto local_x = static_cast<std::int32_t>(tile.x) + 8 - map.center_x - floor_delta;
    const auto local_y = static_cast<std::int32_t>(tile.y) + 6 - map.center_y - floor_delta;
    const auto local_floor = map.center_floor < 8
        ? 7 - static_cast<std::int32_t>(map.center_floor) + floor_delta
        : floor_delta + 2;
    if (local_x < 0 || local_x >= map_viewport_tiles_x ||
        local_y < 0 || local_y >= map_viewport_tiles_y ||
        local_floor < 0 || local_floor > 7)
        return std::nullopt;
    return ViewportMapPosition{
        static_cast<std::uint8_t>(local_x),
        static_cast<std::uint8_t>(local_y),
        static_cast<std::uint8_t>(local_floor)
    };
}

}
