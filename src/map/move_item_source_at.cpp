#include "map/move_item_source_at.hpp"

#include "map/map_position_at.hpp"
#include "map/tile_object_order.hpp"
#include "resources/tibia_dat.hpp"

#include <array>
#include <vector>

namespace tibia::map {
namespace {

std::optional<bool> has_native_property(const Object& object,
    const resources::TibiaDatFile& dat, std::uint8_t property) {
    const auto* item = dat.find_item(object.type);
    if (!item) return std::nullopt;
    return item->has_native_property(property);
}

std::optional<std::size_t> select_native_move_source_stack_position(
    const std::vector<const Object*>& objects,
    const resources::TibiaDatFile& dat) {
    if (objects.empty()) return std::nullopt;

    std::size_t stack_position{};
    while (stack_position + 1 < objects.size()) {
        const auto& object = *objects[stack_position];
        constexpr std::array<std::uint8_t, 4> blocking_properties{0, 1, 2, 3};
        bool skip_object = object.type == 0x63;
        if (!skip_object) {
            for (const auto property : blocking_properties) {
                const auto has_property = has_native_property(object, dat, property);
                if (!has_property) return std::nullopt;
                if (*has_property) {
                    skip_object = true;
                    break;
                }
            }
        }
        if (!skip_object) break;
        ++stack_position;
    }

    if (stack_position > 0) {
        const auto& previous = *objects[stack_position - 1];
        const auto immovable = has_native_property(previous, dat, 0x0e);
        if (!immovable) return std::nullopt;
        if (*immovable) --stack_position;
    }
    return stack_position;
}

}

std::optional<MoveItemSource> move_item_source_at(
    const client::GameWorldSnapshot& snapshot,
    const resources::TibiaDatFile& dat,
    const client::MapWindowLayout& viewport,
    client::MapWindowCameraOffset camera_offset,
    bool no_stretching,
    std::int32_t click_x,
    std::int32_t click_y) {
    const auto position = map_position_at(snapshot, dat, viewport, camera_offset,
        no_stretching, click_x, click_y);
    if (!position || !snapshot.map) return std::nullopt;

    const Tile* selected_tile{};
    for (const auto& tile : snapshot.map->tiles) {
        if (tile.x != position->x || tile.y != position->y ||
            tile.floor != position->floor)
            continue;
        if (selected_tile) return std::nullopt;
        selected_tile = &tile;
    }
    if (!selected_tile) return std::nullopt;

    const auto objects = resolve_native_tile_object_order(*selected_tile, dat);
    if (!objects) return std::nullopt;
    const auto stack_position =
        select_native_move_source_stack_position(*objects, dat);
    if (!stack_position || *stack_position >= objects->size() ||
        *stack_position > 0xff)
        return std::nullopt;
    const auto* object = (*objects)[*stack_position];
    if (object->type == 0 || object->type == 0x63)
        return std::nullopt;
    return MoveItemSource{selected_tile, object, *stack_position};
}

}
