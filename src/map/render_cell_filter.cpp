#include "map/render_cell_filter.hpp"
#include "map/item_render_traversal.hpp"

#include "resources/tibia_dat.hpp"

#include <array>
#include <stdexcept>

namespace tibia::map {
namespace {

inline constexpr std::int32_t map_render_floors = map_render_maximum_floor + 1;
inline constexpr std::size_t map_render_grid_size = map_render_cell_count * map_render_floors;

std::size_t grid_index(std::int32_t local_x, std::int32_t local_y) noexcept {
    return static_cast<std::size_t>(local_x * 14 + local_y);
}

std::size_t floor_grid_index(std::int32_t local_x, std::int32_t local_y,
    std::int32_t floor) noexcept {
    return static_cast<std::size_t>(floor * 18 * 14 + local_x * 14 + local_y);
}

std::optional<bool> object_has_property(const Object& object,
    const resources::TibiaDatFile& dat, std::uint8_t property_id) {
    if (object.creature_id != 0) return property_id == 13;
    if (object.type == 0) return false;
    const auto* item = dat.find_item(object.type);
    if (item == nullptr) return std::nullopt;
    return item->has_native_property(property_id);
}

}

std::optional<std::int32_t> MapRenderFilterGrid::value_at(
    std::int32_t local_x, std::int32_t local_y) const noexcept {
    if (local_x < 0 || local_x >= 18 || local_y < 0 || local_y >= 14)
        return std::nullopt;
    return values[grid_index(local_x, local_y)];
}

MapRenderFilterGrid compute_map_render_filter_grid(
    const DecodedMap& map, const resources::TibiaDatFile& dat,
    std::int32_t highest_active_floor) {
    if (highest_active_floor < 0 || highest_active_floor > map_render_maximum_floor)
        throw std::out_of_range("active map floor is outside the observed render range");

    std::array<const Tile*, map_render_grid_size> tiles{};
    std::array<bool, map_render_grid_size> duplicate_tiles{};
    for (const auto& tile : map.tiles) {
        const auto position = viewport_map_position(map, tile);
        if (!position) continue;
        const auto index = floor_grid_index(position->x, position->y, position->floor);
        if (tiles[index] != nullptr || duplicate_tiles[index]) {
            tiles[index] = nullptr;
            duplicate_tiles[index] = true;
        } else {
            tiles[index] = &tile;
        }
    }

    MapRenderFilterGrid grid;
    for (std::int32_t local_x = 0; local_x < 18; ++local_x) {
        for (std::int32_t local_y = 0; local_y < 14; ++local_y) {
            auto value = std::optional<std::int32_t>{0};
            for (auto floor = highest_active_floor; floor > 0; --floor) {
                const auto index = floor_grid_index(local_x, local_y, floor);
                if (duplicate_tiles[index]) {
                    value.reset();
                    break;
                }
                const auto* tile = tiles[index];
                if (tile == nullptr) continue;
                const auto objects = resolve_native_tile_object_order(*tile, dat);
                if (!objects) {
                    value.reset();
                    break;
                }
                if (objects->empty()) continue;
                const auto& object = *objects->front();
                const auto has_floor_property = object_has_property(object, dat, 0x1f);
                if (!has_floor_property) {
                    value.reset();
                    break;
                }
                if (*has_floor_property) {
                    value = floor;
                    break;
                }
            }
            grid.values[grid_index(local_x, local_y)] = value;
        }
    }
    return grid;
}

std::optional<std::int32_t> compute_highest_active_render_floor(
    const DecodedMap& map, const resources::TibiaDatFile& dat) {
    std::array<const Tile*, map_render_grid_size> tiles{};
    std::array<bool, map_render_grid_size> duplicate_tiles{};
    for (const auto& tile : map.tiles) {
        const auto position = viewport_map_position(map, tile);
        if (!position) continue;
        const auto index = floor_grid_index(position->x, position->y, position->floor);
        if (tiles[index] != nullptr || duplicate_tiles[index]) {
            tiles[index] = nullptr;
            duplicate_tiles[index] = true;
        } else {
            tiles[index] = &tile;
        }
    }

    auto highest_active_floor = map_render_maximum_floor;
    const auto center_floor = map.center_floor < 8
        ? 7 - static_cast<std::int32_t>(map.center_floor)
        : 2;

    for (std::int32_t local_x = 7; local_x < 10; ++local_x) {
        for (std::int32_t local_y = 5; local_y < 8; ++local_y) {
            if (local_x != 8 && local_y != 6) continue;
            const auto base_index = floor_grid_index(local_x, local_y, center_floor);
            if (duplicate_tiles[base_index]) return std::nullopt;

            std::optional<bool> has_floor_blocker = false;
            if (const auto* base_tile = tiles[base_index]) {
                for (const auto& object : base_tile->objects) {
                    const auto has_property = object_has_property(object, dat, 15);
                    if (!has_property) return std::nullopt;
                    if (*has_property) {
                        has_floor_blocker = true;
                        break;
                    }
                }
            }
            if (*has_floor_blocker) continue;

            for (auto previous_floor = center_floor; previous_floor < 7; ++previous_floor) {
                const auto floor = previous_floor + 1;
                const auto shifted_x = local_x - floor + center_floor;
                const auto shifted_y = local_y - floor + center_floor;
                if (shifted_x < 0 || shifted_y < 0) break;

                if (previous_floor < highest_active_floor) {
                    const auto shifted_index = floor_grid_index(shifted_x, shifted_y, floor);
                    if (duplicate_tiles[shifted_index]) return std::nullopt;
                    const auto* shifted_tile = tiles[shifted_index];
                    const auto shifted_objects = shifted_tile == nullptr
                        ? std::optional<std::vector<const Object*>>{
                            std::vector<const Object*>{}}
                        : resolve_native_tile_object_order(*shifted_tile, dat);
                    if (!shifted_objects) return std::nullopt;
                    const auto has_ground = shifted_objects->empty()
                        ? std::optional<bool>{false}
                        : object_has_property(*shifted_objects->front(), dat, 0);
                    if (!has_ground) return std::nullopt;
                    if (*has_ground) {
                        const auto is_special = object_has_property(
                            *shifted_objects->front(), dat, 23);
                        if (!is_special) return std::nullopt;
                        if (!*is_special) highest_active_floor = previous_floor;
                    }

                    if (previous_floor < highest_active_floor) {
                        const auto cell_index = floor_grid_index(local_x, local_y, floor);
                        if (duplicate_tiles[cell_index]) return std::nullopt;
                        const auto* cell_tile = tiles[cell_index];
                        if (cell_tile != nullptr && !cell_tile->objects.empty()) {
                            const auto objects = resolve_native_tile_object_order(*cell_tile, dat);
                            if (!objects || objects->empty()) return std::nullopt;
                            const auto has_property_zero = object_has_property(*objects->front(), dat, 0);
                            const auto has_property_two = object_has_property(*objects->front(), dat, 2);
                            const auto is_special = object_has_property(*objects->front(), dat, 23);
                            if (!has_property_zero || !has_property_two || !is_special)
                                return std::nullopt;
                            if ((*has_property_zero || *has_property_two) && !*is_special)
                                highest_active_floor = previous_floor;
                        }
                    }
                }
            }
        }
    }

    return highest_active_floor;
}

}
