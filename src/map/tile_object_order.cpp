#include "map/tile_object_order.hpp"

#include "resources/tibia_dat.hpp"

namespace tibia::map {

std::optional<std::vector<const Object*>> resolve_native_tile_object_order(
    const Tile& tile, const resources::TibiaDatFile& dat) {
    std::size_t ordered_prefix = 0;
    for (; ordered_prefix < tile.objects.size(); ++ordered_prefix) {
        const auto& object = tile.objects[ordered_prefix];
        if (object.creature_id != 0 || object.type == 0 || object.type == 0x63)
            break;
        const auto* item = dat.find_item(object.type);
        if (item == nullptr) return std::nullopt;
        if (!item->has_native_property(0) && !item->has_native_property(1) &&
            !item->has_native_property(2))
            break;
    }

    std::vector<const Object*> objects;
    objects.reserve(tile.objects.size());
    for (std::size_t index = 0; index < ordered_prefix; ++index)
        objects.push_back(&tile.objects[index]);
    for (std::size_t index = tile.objects.size(); index > ordered_prefix; --index)
        objects.push_back(&tile.objects[index - 1]);
    return objects;
}

bool refresh_native_tile_object_context(Tile& tile,
    const resources::TibiaDatFile& dat) noexcept {
    std::optional<std::size_t> hanging_object;
    std::optional<std::size_t> wall_object;
    bool wall_object_has_property_19 = false;
    for (std::size_t index = tile.objects.size(); index > 0; --index) {
        auto& object = tile.objects[index - 1];
        if (object.creature_id != 0 || object.type == 0 || object.type == 0x63)
            continue;
        const auto* item = dat.find_item(object.type);
        if (item == nullptr) return false;
        if (item->has_native_property(18)) hanging_object = index - 1;
        const auto has_property_19 = item->has_native_property(19);
        if (has_property_19 || item->has_native_property(20)) {
            wall_object = index - 1;
            wall_object_has_property_19 = has_property_19;
        }
    }
    if (hanging_object && wall_object)
        tile.objects[*hanging_object].render_context =
            wall_object_has_property_19 ? 19U : 20U;
    return true;
}

}
